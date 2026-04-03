#include "shell_api.hpp"

#include <sstream>
#include <string>
#include <cctype>
#include <iostream>
#include <vector>
#include <iomanip>
#include <cmath>

#include "xbase.hpp"
#include "command_registry.hpp"
#include "textio.hpp"
#include "shell_shortcuts.hpp"
#include "shell_var_utils.hpp"
#include "shell_control_utils.hpp"
#include "shell_eval_utils.hpp"
#include "shell.hpp"
#include "shell_api_extras.hpp"
#include "cli/expr/value_eval.hpp"
#include "cli/rel_refresh_suppress.hpp"

#if __has_include("cli/path_resolver.hpp") && __has_include("cli/cmd_setpath.hpp")
  #include "cli/path_resolver.hpp"
  #include "cli/cmd_setpath.hpp"
  #define HAVE_SCRIPT_PATHS 1
#else
  #define HAVE_SCRIPT_PATHS 0
#endif

using dli::registry;

namespace {
    static inline bool is_space(unsigned char c) {
        return (c==' '||c=='\t'||c=='\r'||c=='\n');
    }

    static inline std::string trim(std::string s) {
        while (!s.empty() && is_space((unsigned char)s.front())) s.erase(s.begin());
        while (!s.empty() && is_space((unsigned char)s.back()))  s.pop_back();
        return s;
    }

    static bool begins_with_comment(const std::string& raw) {
        const std::string s = trim(raw);
        if (s.empty()) return false;
        if (s[0] == '#') return true;
        if (s.size() >= 2 && s[0] == '/' && s[1] == '/') return true;
        return false;
    }

    static std::string expand_shortcut_lead(const std::string& s) {
        std::istringstream iss(s);
        std::string first; iss >> first;
        if (first.empty()) return s;
        const std::string fullCmd = shell_shortcuts::resolve(first);
        if (fullCmd == first) return s;
        size_t argStart = s.find_first_not_of(" \t", first.size());
        if (argStart == std::string::npos) return fullCmd;
        std::string args = s.substr(argStart);
        size_t j = 0;
        while (j < args.size() && std::isspace(static_cast<unsigned char>(args[j]))) ++j;
        if (j) args.erase(0, j);
        return fullCmd + (args.empty() ? "" : (" " + args));
    }

    static bool is_ident_like_expr(const std::string& s) {
        if (s.empty()) return false;
        const unsigned char c0 = static_cast<unsigned char>(s[0]);
        if (!(std::isalpha(c0) || s[0] == '_' || s[0] == '$')) return false;
        for (char c : s) {
            const unsigned char uc = static_cast<unsigned char>(c);
            if (!(std::isalnum(uc) || c == '_' || c == '$')) return false;
        }
        return true;
    }

    static int field_index_ci_expr(const xbase::DbArea& area, std::string_view name) {
        const auto& Fs = area.fields();
        std::string want(name);
        want = textio::up(textio::trim(want));
        for (int i = 0; i < static_cast<int>(Fs.size()); ++i) {
            std::string got = textio::up(textio::trim(Fs[static_cast<size_t>(i)].name));
            if (got == want) return i + 1;
        }
        return 0;
    }

    static std::string format_eval_number(double v)
    {
        const double iv = std::floor(v);
        if (std::fabs(v - iv) < 1e-9) {
            std::ostringstream o;
            o << static_cast<long long>(iv);
            return o.str();
        }
        std::ostringstream o;
        o << std::fixed << std::setprecision(10) << v;
        std::string s = o.str();
        while (!s.empty() && s.find('.') != std::string::npos && s.back() == '0') s.pop_back();
        if (!s.empty() && s.back() == '.') s.pop_back();
        return s.empty() ? "0" : s;
    }

    static bool looks_like_expression_candidate(xbase::DbArea& area, const std::string& raw) {
        const std::string s = trim(raw);
        if (s.empty()) return false;

        if (s == ".T." || s == ".F.") return true;

        const char c0 = s.front();
        if (c0 == '"' || c0 == '\'') return true;
        if (std::isdigit(static_cast<unsigned char>(c0))) return true;
        if ((c0 == '+' || c0 == '-' || c0 == '.') &&
            s.size() > 1 &&
            std::isdigit(static_cast<unsigned char>(s[1]))) return true;

        for (char c : s) {
            switch (c) {
                case '(':
                case ')':
                case ',':
                case '+':
                case '-':
                case '*':
                case '/':
                case '=':
                case '<':
                case '>':
                case '"':
                case '\'':
                    return true;
                default:
                    break;
            }
        }

        if (area.isOpen() && is_ident_like_expr(s) && field_index_ci_expr(area, s) > 0)
            return true;

        return false;
    }

    static bool try_shell_expression_fallback(xbase::DbArea& area,
                                              const std::string& expr,
                                              bool print_result)
    {
        const std::string s = trim(expr);
        if (!looks_like_expression_candidate(area, s)) return false;

        std::string err;
        const auto ev = dottalk::expr::eval_any(area, s, &err);

        using EV = dottalk::expr::EvalValue;
        switch (ev.kind) {
            case EV::K_String:
                if (print_result) std::cout << ev.text << "\n";
                return true;
            case EV::K_Number:
                if (print_result) std::cout << format_eval_number(ev.number) << "\n";
                return true;
            case EV::K_Bool:
                if (print_result) std::cout << (ev.tf ? ".T." : ".F.") << "\n";
                return true;
            default:
                return false;
        }
    }
}

namespace {
    struct ScriptFrame {
        std::filesystem::path file;
        std::filesystem::path dir;
        bool subscript = false;
    };

    static std::vector<ScriptFrame>& script_stack() {
        static std::vector<ScriptFrame> st;
        return st;
    }

    static std::filesystem::path normalize_script_path(std::filesystem::path p) {
        p = p.lexically_normal();
        try {
            if (std::filesystem::exists(p)) return std::filesystem::weakly_canonical(p);
        } catch (...) {}
        return p;
    }
}

std::filesystem::path shell_script_current_dir()
{
    auto& st = script_stack();
    return st.empty() ? std::filesystem::path{} : st.back().dir;
}

bool shell_script_active()
{
    return !script_stack().empty();
}

bool shell_script_in_subscript()
{
    auto& st = script_stack();
    return !st.empty() && st.back().subscript;
}

std::filesystem::path shell_resolve_script_path(const std::string& token)
{
    namespace fs = std::filesystem;
    fs::path p(token);
    if (!p.has_extension()) p.replace_extension(".dts");

    auto try_existing = [](const fs::path& c) -> fs::path {
        try {
            if (fs::exists(c) && fs::is_regular_file(c)) return normalize_script_path(c);
        } catch (...) {}
        return {};
    };

    if (p.is_absolute()) return normalize_script_path(p);

    if (shell_script_active()) {
        if (auto c = try_existing(shell_script_current_dir() / p); !c.empty()) return c;
        return normalize_script_path(shell_script_current_dir() / p);
    }

#if HAVE_SCRIPT_PATHS
    try {
        const fs::path scriptsRoot = dottalk::paths::get_slot(dottalk::paths::Slot::SCRIPTS);
        if (auto c = try_existing(scriptsRoot / p); !c.empty()) return c;
    } catch (...) {}
#endif

    return normalize_script_path(fs::current_path() / p);
}

bool shell_script_push(const std::filesystem::path& scriptFile, bool as_subscript, std::string* err)
{
    auto& st = script_stack();
    const auto full = normalize_script_path(scriptFile);

    if (as_subscript && !st.empty() && st.back().subscript) {
        if (err) *err = "nested subscripts are not allowed";
        return false;
    }
    for (const auto& f : st) {
        if (normalize_script_path(f.file) == full) {
            if (err) *err = "self-call/recursive script call is not allowed";
            return false;
        }
    }
    st.push_back(ScriptFrame{full, full.parent_path(), as_subscript});
    return true;
}

void shell_script_pop()
{
    auto& st = script_stack();
    if (!st.empty()) st.pop_back();
}

// Low-level dispatch of a single already-prepared command line through the registry.
// Kept for compatibility with existing callers and tests.
bool shell_dispatch(xbase::DbArea& area, const std::string& rawLine)
{
    // Skip blank / comment lines
    std::string line = trim(rawLine);
    if (line.empty()) return true;
    if (line[0] == '#') return true;
    if (line.size() >= 2 && line[0]=='/' && line[1]=='/') return true;

    // Expand first token via shortcut resolver
    line = cli::preprocess_for_dispatch(expand_shortcut_lead(line));

    std::istringstream tok(line);
    std::string cmd;
    tok >> cmd;
    if (cmd.empty()) return true;

    const std::string U = textio::up(cmd);
    if (!registry().run(area, U, tok)) {
        if (try_shell_expression_fallback(area, line, false))
            return true;
        return false;
    }
    return true;
}

// Canonical shell execution path. All front-ends should call this.
bool shell_execute_line(xbase::DbArea& area, const std::string& rawLine)
{
    if (begins_with_comment(rawLine)) return true;

    std::string trimmed = trim(rawLine);
    if (trimmed.empty()) return true;

    std::string prepared = cli::preprocess_for_dispatch(expand_shortcut_lead(trimmed));

    std::istringstream tok0(prepared);
    std::string cmdToken0;
    tok0 >> cmdToken0;
    if (cmdToken0.empty()) return true;

    const std::string U0 = textio::up(cmdToken0);
    if (dottalk::shell_if_is_suppressed() && !dottalk::is_if_control_command(U0))
        return true;

    {
        dottalk::VarCmdResult vr = dottalk::try_handle_var_command(area, prepared);
        if (vr.handled) return vr.ok;
    }

    std::string macroLine;
    std::string errName;
    if (!dottalk::expand_macros_outside_quotes(prepared, macroLine, errName)) {
        std::cout << "MACRO: undefined variable: " << errName << "\n";
        return false;
    }

    std::istringstream tok(macroLine);
    std::string cmdToken;
    tok >> cmdToken;
    if (cmdToken.empty()) return true;

    const std::string U = textio::up(cmdToken);

    RelRefreshGuard guard(shell_is_rel_refresh_suppression_command(U));
    if (!registry().run(area, U, tok)) {
        if (try_shell_expression_fallback(area, macroLine, true))
            return true;
        std::cout << "Unknown command: " << cmdToken << "\n";
        return false;
    }

    return true;
}

// Public API wrapper expected by dispatch_shim and tests.
bool shell_dispatch_line(xbase::DbArea& area, const std::string& line)
{
    return shell_dispatch(area, line);
}
