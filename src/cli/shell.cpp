// ==============================
// File: src/cli/shell.cpp
// Auto-refresh tuned for record cursor cohesion:
// - Rely on engine cursor hook for *cursor moves* (avoid double refresh).
// - Keep manual refresh only where cursor hook may not fire (SELECT/USE/CLOSE,
// relation definition changes, and data mutations that may affect join keys).
// - Do NOT auto-refresh after COUNT/LIST/DISPLAY (they should be non-positioning).
// ==============================
#include <iostream>
#include <sstream>
#include <string>
#include <string_view>
#include <cstdlib>
#include <array>
#include <cctype>
#include <istream>
#include <algorithm>
#include <vector>
#include <unordered_map>
#include <locale>
#include <iomanip>
#include <ctime>
#include <cmath>
#include <charconv>
#include <chrono>

#include "cli/expr/api.hpp"
#include "cli/expr/ast.hpp"
#include "cli/expr/glue_xbase.hpp"
#include "cli/expr/value_eval.hpp"  // for EvalValue and eval_any
#include "cli/shell_lexicon.hpp"
#include "cli/output_router.hpp"

// Indexing
#include <lmdb.h>

// top include
#include "xbase.hpp"
#include "xbase_vfp.hpp"
#include "xbase_64.hpp"
#include "textio.hpp"

// misc
#include "colors.hpp"
#include "scan_state.hpp"
#include "scan_capture_hook.hpp"
#include "command_registry.hpp"
#include "loop_state.hpp"
#include "reserved_words.hpp"

#include "cmd_version.hpp"
#include "cmd_help.hpp"
#include "cmd_dbarea.hpp"
#include "cmd_scan.hpp"
#include "cmd_loop.hpp"

// tvision
#include "cmd_fox_palette_command.h"

// relationships
#include "set_relations.hpp"

#include "dli/set_view.hpp"

// commands
#include "cli/cmd_dbarea.hpp"
#include "cli/cmd_aggs.hpp"
#include "cli/cmd_replace_multi.hpp"
#include "cli/order_state.hpp"
#include "cli/cmd_table.hpp"
#include "cli/dirty_prompt.hpp"
#include "cli/rel_refresh_suppress.hpp"
#include "cli/settings.hpp"
#include "cmd_polling.hpp"
#include "shell_commands.hpp"
#include "shell_shortcuts.hpp"
#include "shell_bool_eval_adapter.hpp"
#include "shell_control_utils.hpp"
#include "shell_buffer_utils.hpp"
#include "shell_var_utils.hpp"
#include "shell_eval_utils.hpp"
#include "shell_api_extras.hpp"
#include "shell_api.hpp"
#include "cli/script_reader.hpp"

// NEW: date utilities
#include "expr/date/date_utils.hpp"
#include "expr/date/date_arith.hpp"

// engine-level cursor hook
#include "../xbase/cursor_hook.hpp"

extern "C" void while_set_condition_eval(bool(*)(xbase::DbArea&, const std::string&));
extern "C" void until_set_condition_eval(bool(*)(xbase::DbArea&, const std::string&));

// Private buffer helpers/state probes for buffered control-family replay
void cmd_WHILE_BUFFER(xbase::DbArea& A, std::istringstream& S);
void cmd_UNTIL_BUFFER(xbase::DbArea& A, std::istringstream& S);
extern "C" bool while_is_active();
extern "C" bool until_is_active();

#ifdef _WIN32
#include <io.h>
#else
#include <unistd.h>
#endif

char prompt_char = '.';

// -----------------------------------------------------------------------------
// Shell block-capture state
// NOTE:
//   After shell_execute_line unification, structured control blocks like
//   IF/LOOP/SCAN/WHILE/UNTIL must be handled by their own command/state logic.
//   The shell-level block capture remains only for DO/ENDDO shell-only grouping.
// -----------------------------------------------------------------------------
struct BlockCaptureState
{
    bool active = false;
    std::string begin_token;
    bool shell_only = false;              // DO/ENDDO are handled in the shell
    std::vector<std::string> lines;       // captured physical/logical command lines
    std::vector<std::string> end_stack;   // nesting stack of expected end tokens
};

static BlockCaptureState g_block;

struct LoopCaptureState
{
    bool active = false;
    std::string end_token;
    std::vector<std::string> lines;
};

static LoopCaptureState g_loopcap;

// ------------------------------------------------------------
// Simple std::cout buffering (CLI performance).
// ------------------------------------------------------------
namespace dt_cli_outbuf {
    static bool g_enabled = false;
    static std::vector<char> g_buf;

    static void enable(std::size_t bytes = 256 * 1024) {
        if (g_enabled) return;
        g_buf.assign(bytes, '\0');
        std::cout.rdbuf()->pubsetbuf(g_buf.data(),
                                     static_cast<std::streamsize>(g_buf.size()));
        g_enabled = true;
    }

    static inline void flush_prompt() {
        std::cout.flush();
    }
}

namespace {

// -----------------------------------------------------------------------------
// Existing helpers
// -----------------------------------------------------------------------------
static bool leading_command_is_sqlite(const std::string& s)
{
    size_t i = 0;
    while (i < s.size() && std::isspace(static_cast<unsigned char>(s[i]))) ++i;
    if (i >= s.size()) return false;
    if (s[i] == prompt_char) {
        size_t j = i + 1;
        if (j < s.size() && std::isspace(static_cast<unsigned char>(s[j]))) {
            i = j;
            while (i < s.size() && std::isspace(static_cast<unsigned char>(s[i]))) ++i;
            if (i >= s.size()) return false;
        }
    }
    size_t j = i;
    while (j < s.size() && !std::isspace(static_cast<unsigned char>(s[j]))) ++j;
    if (j <= i) return false;
    const std::string first = s.substr(i, j - i);
    return textio::up(first) == "SQLITE";
}

bool last_semicolon_is_outside_quotes(const std::string& s)
{
    if (leading_command_is_sqlite(s)) return false;
    bool in_single = false, in_double = false, esc = false;
    for (char c : s) {
        if (esc) { esc = false; continue; }
        if (c == '\\') { esc = true; continue; }
        if (!in_double && c == '\'') { in_single = !in_single; continue; }
        if (!in_single && c == '\"') { in_double = !in_double; continue; }
    }
    if (in_single || in_double) return false;
    size_t i = s.find_last_not_of(" \t\r");
    return i != std::string::npos && s[i] == ';';
}

std::string strip_hash_comment(const std::string& s) {
    bool in_single = false, in_double = false, esc = false;
    for (size_t i = 0; i < s.size(); ++i) {
        char c = s[i];
        if (esc) { esc = false; continue; }
        if (c == '\\') { esc = true; continue; }
        if (!in_double && c == '\'') { in_single = !in_single; continue; }
        if (!in_single && c == '\"') { in_double = !in_double; continue; }
        if (!in_single && !in_double && c == '#') {
            size_t j = (i == 0) ? 0 : i;
            while (j > 0 && (s[j-1] == ' ' || s[j-1] == '\t')) --j;
            return s.substr(0, j);
        }
    }
    return s;
}

bool read_command_multiline(std::istream& in, std::string& out) {
    out.clear();
    std::string line;
    if (!std::getline(in, line)) return false;
    line = strip_hash_comment(line);
    std::string accum = line;
    while (last_semicolon_is_outside_quotes(accum)) {
        while (!accum.empty() &&
               (accum.back()==' ' || accum.back()=='\t' || accum.back()=='\r' || accum.back()==';')) {
            char c = accum.back();
            accum.pop_back();
            if (c == ';') break;
        }
        std::string more;
        if (!std::getline(in, more)) break;
        more = strip_hash_comment(more);
        if (!accum.empty() && !more.empty()) accum.push_back(' ');
        accum += more;
    }
    out = accum;
    return true;
}

bool begins_with_comment(const std::string& s) {
    size_t i = 0;
    while (i < s.size() && std::isspace(static_cast<unsigned char>(s[i]))) ++i;
    if (i >= s.size()) return false;
    if (s[i] == '#') return true;
    if (s[i] == '*') return true;
    return (s[i] == '/' && i + 1 < s.size() && s[i + 1] == '/');
}

std::string expand_shortcut_lead(const std::string& s) {
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

inline bool is_match(const std::string& u, const char* a, const char* b) {
    return u == a || u == b;
}

static std::string first_token(const std::string& line)
{
    std::istringstream iss(line);
    std::string tok;
    iss >> tok;
    return textio::up(tok);
}

static bool is_cancel_token(const std::string& U)
{
    return (U == "CANCEL" || U == "ABORT");
}

// -----------------------------------------------------------------------------
// IMPORTANT:
// shell-level block capture is now ONLY for DO/ENDDO.
// Structured control commands are handled by their own command/state logic.
// -----------------------------------------------------------------------------
static bool block_begin_token(const std::string& U, std::string& endtok, bool& shell_only)
{
    shell_only = false;

    if (U == "DO") {
        endtok = "ENDDO";
        shell_only = true;
        return true;
    }

    return false;
}

} // namespace

// -----------------------------------------------------------------------------
// Shell engine pointer API
// -----------------------------------------------------------------------------
static xbase::XBaseEngine* g_shell_engine = nullptr;
extern "C" xbase::XBaseEngine* shell_engine() { return g_shell_engine; }

// -----------------------------------------------------------------------------
// Relations auto-refresh suppression
// -----------------------------------------------------------------------------
static thread_local int g_rel_refresh_suppress = 0;

// -----------------------------------------------------------------------------
// Cursor-change hook
// -----------------------------------------------------------------------------
static void on_cursor_changed(xbase::DbArea& moved, const char* /*reason*/, void* user) noexcept {
    if (shell_rel_refresh_is_suspended()) return;
    auto* eng = static_cast<xbase::XBaseEngine*>(user);
    if (!eng) return;
    try {
        xbase::DbArea& cur = eng->area(eng->currentArea());
        if (&cur != &moved) return;
    } catch (...) { return; }
    relations_api::refresh_if_enabled();
}

// -----------------------------------------------------------------------------
// Helpers
// -----------------------------------------------------------------------------
static inline std::string basename_upper(std::string path) {
    std::replace(path.begin(), path.end(), '\\', '/');
    auto slash = path.find_last_of('/');
    if (slash != std::string::npos) path.erase(0, slash + 1);
    auto dot = path.find_last_of('.');
    if (dot != std::string::npos) path.erase(dot);
    std::transform(path.begin(), path.end(), path.begin(),
                   [](unsigned char c){ return char(std::toupper(c)); });
    return path;
}

static inline bool is_digits(const std::string& s) {
    if (s.empty()) return false;
    for (unsigned char c : s) if (!std::isdigit(c)) return false;
    return true;
}

/*
static int resolve_area_index_by_name(xbase::XBaseEngine& eng, const std::string& tokRaw) {
    std::string tok = textio::trim(tokRaw);
    if (tok.empty()) return -1;
    if (is_digits(tok)) {
        int n = 0; try { n = std::stoi(tok); } catch (...) { return -1; }
        if (n >= 0 && n < xbase::MAX_AREA) return n;
        return -1;
    }
    const std::string want = textio::up(tok);
    for (int i = 0; i < xbase::MAX_AREA; ++i) {
        xbase::DbArea& A = eng.area(i);
        if (!A.isOpen()) continue;
        const std::string base = basename_upper(A.name());
        if (base == want) return i;
    }
    return -1;
}
*/

// -----------------------------------------------------------------------------
// Buffer handling
// -----------------------------------------------------------------------------
// NOTE:
//   This interception must happen BEFORE generic shell block capture and before
//   normal execution. Otherwise loop/scan body lines leak through and execute
//   once immediately instead of being buffered.
// -----------------------------------------------------------------------------
static bool handle_buffers_if_active(xbase::XBaseEngine& eng,
                                     const std::string& U,
                                     const std::string& line_for_scan,
                                     const std::string& line_for_loop)
{
    using namespace dli;

    if (scanblock::state().active && !is_match(U, "ENDSCAN", "END SCAN")) {
        xbase::DbArea& curCap = eng.area(eng.currentArea());
        std::istringstream cap(line_for_scan);
        registry().run(curCap, "SCAN_BUFFER", cap);
        return true;
    }

    if (loopblock::state().active &&
        !is_match(U, "ENDLOOP", "END LOOP") &&
        !is_match(U, "ENDWHILE", "END WHILE") &&
        !is_match(U, "ENDUNTIL", "END UNTIL")) {
        xbase::DbArea& curCap = eng.area(eng.currentArea());
        std::istringstream cap(line_for_loop);
        registry().run(curCap, "LOOP_BUFFER", cap);
        return true;
    }

    return false;
}

// -----------------------------------------------------------------------------
// Loop execution
// -----------------------------------------------------------------------------
static void loop_exec_line(xbase::DbArea& area, const std::string& rawLine)
{
    (void)shell_execute_line(area, rawLine);
}

// -----------------------------------------------------------------------------
// BROWSETUI dispatch
// -----------------------------------------------------------------------------
[[maybe_unused]] static void browsetui_dispatch_line(xbase::DbArea& area, const std::string& rawLine)
{
    (void)shell_execute_line(area, rawLine);
}

static void execute_shell_only_block_lines(xbase::DbArea& area,
                                           const std::vector<std::string>& lines,
                                           std::size_t start,
                                           std::size_t end_exclusive)
{
    for (std::size_t i = start; i < end_exclusive; ++i) {
        const std::string tok = first_token(lines[i]);

        std::string endtok;
        bool shell_only = false;
        if (block_begin_token(tok, endtok, shell_only) && shell_only) {
            int depth = 1;
            std::size_t j = i + 1;
            for (; j < end_exclusive; ++j) {
                const std::string innerTok = first_token(lines[j]);
                std::string innerEnd;
                bool innerShellOnly = false;
                if (block_begin_token(innerTok, innerEnd, innerShellOnly) && innerShellOnly) {
                    ++depth;
                    continue;
                }
                if (innerTok == endtok) {
                    --depth;
                    if (depth == 0) break;
                }
            }
            if (j > i + 1) {
                execute_shell_only_block_lines(area, lines, i + 1, j);
            }
            i = j;
            continue;
        }

        if (tok == "ENDDO") continue;

        browsetui_dispatch_line(area, lines[i]);
    }
}

static void shell_execute_instrumented(xbase::DbArea& cur,
                                       const std::string& expandedLine)
{
    using timer_clock = std::chrono::steady_clock;
    static const auto shell_timer_base = timer_clock::now();

    auto& S = cli::Settings::instance();
    const bool timer_on   = S.timer_on.load();
    const bool polling_on = S.polling_on.load();

    if (timer_on) {
        const auto t0 = timer_clock::now();
        const double t0_sec =
            std::chrono::duration<double>(t0 - shell_timer_base).count();

        if (polling_on) {
            pre_poll();
        }

        std::cout << std::fixed << std::setprecision(9)
                  << "TIMER START: " << t0_sec << " s\n";

        (void)shell_execute_line(cur, expandedLine);

        if (polling_on) {
            post_poll();
        }

        const auto t1 = timer_clock::now();
        const double t1_sec =
            std::chrono::duration<double>(t1 - shell_timer_base).count();
        const double elapsed_sec =
            std::chrono::duration<double>(t1 - t0).count();

        std::cout << std::fixed << std::setprecision(9)
                  << "TIMER END  : " << t1_sec << " s\n"
                  << "ELAPSED    : " << elapsed_sec << " s\n";
    } else {
        if (polling_on) {
            pre_poll();
        }

        (void)shell_execute_line(cur, expandedLine);

        if (polling_on) {
            post_poll();
        }

        std::cout.flush();
    }
}

// -----------------------------------------------------------------------------
// Main shell loop
// -----------------------------------------------------------------------------
int run_shell()
{
    using namespace xbase;
    using namespace dli;

#ifdef _WIN32
    const bool interactive_shell =
        (_isatty(_fileno(stdin)) != 0) &&
        (_isatty(_fileno(stdout)) != 0);
#else
    const bool interactive_shell =
        (isatty(STDIN_FILENO) != 0) &&
        (isatty(STDOUT_FILENO) != 0);
#endif

    if (!interactive_shell) {
        dt_cli_outbuf::enable();
    }

    colors::applyTheme(colors::Theme::Green);

    XBaseEngine eng;
    eng.selectArea(0);
    g_shell_engine = &eng;

    xbase::cursor_hook::set_callback(&on_cursor_changed, &eng);
    relations_api::attach_engine(&eng);
    relations_api::set_autorefresh(true);

    register_shell_commands(eng, /*include_ui_cmds=*/true);
    shell_eval_register_for_loops();
    loop_set_executor(+loop_exec_line);

    {
        DbArea& cur = eng.area(eng.currentArea());
        std::istringstream empty;
        try { cmd_INIT(cur, empty); } catch (...) { std::cerr << "INIT failed\n"; }
        std::cout.flush();
    }

    // Start app with cleared screen
    // clear();

    std::cout << "DotTalk++ type HELP. USE, SELECT <n>, AREA, COLOR <GREEN|AMBER|DEFAULT>, ABOUT, QUIT.\n\n";
    std::cout << "Hello World!\n";
    std::cout.flush();

    auto& router = cli::OutputRouter::instance();

    struct ShellCommandRouteGuard {
        cli::OutputRouter& router;
        bool active = false;

        ShellCommandRouteGuard(cli::OutputRouter& r, bool interactive)
            : router(r), active(true)
        {
            router.begin_shell_command(interactive);
            router.push_cout_redirect();
        }

        ~ShellCommandRouteGuard()
        {
            if (!active) return;
            router.pop_cout_redirect();
            router.end_shell_command();
        }

        ShellCommandRouteGuard(const ShellCommandRouteGuard&) = delete;
        ShellCommandRouteGuard& operator=(const ShellCommandRouteGuard&) = delete;
    };

//  using timer_clock = std::chrono::steady_clock;
//  static const auto shell_timer_base = timer_clock::now();

    std::string line;
    while (true) {
        if (g_block.active || g_loopcap.active) std::cout << ".. ";
        else std::cout << prompt_char << " ";
        dt_cli_outbuf::flush_prompt();

        if (!read_script_command(std::cin, line)) break;
        if (begins_with_comment(line)) continue;

        std::string trimmed = textio::trim(line);
        if (trimmed.empty()) continue;

        // ------------------------------------------------------------
        // BLOCK CAPTURE MODE (DO/ENDDO only)
        // ------------------------------------------------------------
        if (g_block.active) {
            const std::string tok = first_token(trimmed);

            if (is_cancel_token(tok)) {
                std::cout << "BLOCK: cancelled\n";
                g_block = BlockCaptureState{};
                continue;
            }

            g_block.lines.push_back(trimmed);

            std::string nestedEnd;
            bool nestedShellOnly = false;
            if (block_begin_token(tok, nestedEnd, nestedShellOnly)) {
                g_block.end_stack.push_back(nestedEnd);
                continue;
            }

            if (!g_block.end_stack.empty() && tok == g_block.end_stack.back()) {
                g_block.end_stack.pop_back();

                if (g_block.end_stack.empty()) {
                    DbArea& cur = eng.area(eng.currentArea());
                    if (g_block.shell_only) {
                        if (g_block.lines.size() >= 2) {
                            execute_shell_only_block_lines(cur, g_block.lines, 1, g_block.lines.size() - 1);
                        }
                    } else {
                        for (const auto& L : g_block.lines)
                            browsetui_dispatch_line(cur, L);
                    }
                    g_block = BlockCaptureState{};
                }
                continue;
            }

            continue;
        }

        if (g_loopcap.active) {
            const std::string tok = first_token(trimmed);

            if (is_cancel_token(tok)) {
                std::cout << "LOOP BLOCK: cancelled\n";
                g_loopcap = LoopCaptureState{};
                continue;
            }

            if (tok == g_loopcap.end_token) {
                DbArea& cur = eng.area(eng.currentArea());

                for (const auto& L : g_loopcap.lines) {
                    std::istringstream cap(L);

                    if (g_loopcap.end_token == "ENDWHILE") {
                        cmd_WHILE_BUFFER(cur, cap);
                    } else if (g_loopcap.end_token == "ENDUNTIL") {
                        cmd_UNTIL_BUFFER(cur, cap);
                    } else if (g_loopcap.end_token == "ENDSCAN") {
                        cmd_SCAN_BUFFER(cur, cap);
                    } else {
                        // ENDLOOP
                        cmd_LOOP_BUFFER(cur, cap);
                    }
                }

                ShellCommandRouteGuard route_guard(router, interactive_shell);
                shell_execute_instrumented(cur, trimmed);

                g_loopcap = LoopCaptureState{};
                continue;
            }

            g_loopcap.lines.push_back(trimmed);
            continue;
        }

        const std::string expandedLine = expand_shortcut_lead(trimmed);
        std::istringstream tok(expandedLine);
        std::string cmdToken;
        tok >> cmdToken;
        const std::string U = dottalk::lexicon::normalize_token(cmdToken);
        const auto& tokinfo = dottalk::lexicon::classify_token(U);
        (void)tokinfo;

        // ------------------------------------------------------------
        // IMPORTANT:
        // Loop/scan/while/until body buffering must happen BEFORE any
        // shell-level block capture or normal execution.
        // ------------------------------------------------------------
        if (handle_buffers_if_active(eng, U, expandedLine, expandedLine)) {
            continue;
        }

        std::string endtok;
        bool shell_only = false;
        if (block_begin_token(U, endtok, shell_only)) {
            g_block.active = true;
            g_block.begin_token = U;
            g_block.shell_only = shell_only;
            g_block.lines.clear();
            g_block.lines.push_back(expandedLine);
            g_block.end_stack.clear();
            g_block.end_stack.push_back(endtok);
            continue;
        }

        if (U == "QUIT" || U == "EXIT") {
            if (!dottalk::dirty::maybe_prompt_all(eng, "QUIT")) {
                std::cout << "QUIT canceled.\n";
                continue;
            }
            break;
        }

        DbArea& cur = eng.area(eng.currentArea());

        // Shell-only paging / cout redirection for this one command.
        ShellCommandRouteGuard route_guard(router, interactive_shell);

        shell_execute_instrumented(cur, expandedLine);

        if (!g_loopcap.active) {
            if (U == "WHILE" && while_is_active()) {
                g_loopcap.active = true;
                g_loopcap.end_token = "ENDWHILE";
                g_loopcap.lines.clear();
                continue;
            }
            if (U == "UNTIL" && until_is_active()) {
                g_loopcap.active = true;
                g_loopcap.end_token = "ENDUNTIL";
                g_loopcap.lines.clear();
                continue;
            }
            if (U == "LOOP" && loopblock::state().active) {
                g_loopcap.active = true;
                g_loopcap.end_token = "ENDLOOP";
                g_loopcap.lines.clear();
                continue;
            }
            if (U == "SCAN" && scanblock::state().active) {
                g_loopcap.active = true;
                g_loopcap.end_token = "ENDSCAN";
                g_loopcap.lines.clear();
                continue;
            }
        }
    }

    {
        DbArea& cur = eng.area(eng.currentArea());
        std::istringstream empty;
        try { cmd_SHUTDOWN(cur, empty); } catch (...) { std::cerr << "SHUTDOWN failed\n"; }
        std::cout.flush();
    }

    g_shell_engine = nullptr;
    return 0;
}