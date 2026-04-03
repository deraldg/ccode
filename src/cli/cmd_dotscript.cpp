// src/commands/cmd_dotscript.cpp
// DOTSCRIPT runner with TRACE banner + scripts/tests resolver + @file support + one-level subscript limit.

#include <filesystem>
#include <fstream>
#include <iostream>
#include <optional>
#include <sstream>
#include <string>
#include <vector>

#include "shell_api.hpp"
#include "xbase.hpp"

using xbase::DbArea;

namespace {

struct ScopeExit {
    void (*fn)() = nullptr;
    ~ScopeExit() { if (fn) fn(); }
};

static inline std::string ltrim_copy(std::string s) {
    size_t i = 0;
    while (i < s.size() && (s[i] == ' ' || s[i] == '\t' || s[i] == '\r' || s[i] == '\n')) ++i;
    s.erase(0, i);
    return s;
}

static inline std::string rtrim_copy(std::string s) {
    while (!s.empty()) {
        const char c = s.back();
        if (c == ' ' || c == '\t' || c == '\r' || c == '\n') s.pop_back();
        else break;
    }
    return s;
}

static inline std::string trim_copy(std::string s) {
    return rtrim_copy(ltrim_copy(std::move(s)));
}

static inline std::string upper_copy(std::string s) {
    for (char& c : s) {
        if (c >= 'a' && c <= 'z') c = static_cast<char>(c - 'a' + 'A');
    }
    return s;
}

static inline std::string unquote_copy(std::string s) {
    s = trim_copy(std::move(s));
    if (s.size() >= 2) {
        const char a = s.front();
        const char b = s.back();
        if ((a == '"' && b == '"') || (a == '\'' && b == '\'')) {
            s = s.substr(1, s.size() - 2);
        }
    }
    return trim_copy(std::move(s));
}

static inline std::string strip_at_prefix(std::string s) {
    s = trim_copy(std::move(s));
    if (!s.empty() && s.front() == '@') {
        s.erase(0, 1);
        s = trim_copy(std::move(s));
        s = unquote_copy(std::move(s));
    }
    return s;
}

static inline bool looks_like_comment_or_blank(const std::string& line) {
    const std::string t = ltrim_copy(line);
    if (t.empty()) return true;
    if (t.rfind("*", 0) == 0) return true;
    if (t.rfind("//", 0) == 0) return true;
    if (t.rfind("&&", 0) == 0) return true;
    if (t.rfind(";", 0) == 0) return true;
    return false;
}

static inline bool has_extension(const std::string& s) {
    return std::filesystem::path(s).has_extension();
}

static std::vector<std::string> build_candidate_specs(const std::string& spec) {
    std::vector<std::string> out;
    out.push_back(spec);

    if (!has_extension(spec)) {
        out.push_back(spec + ".dts");
    }

    const std::filesystem::path p(spec);
    const bool no_parent = !p.has_parent_path();

    if (no_parent) {
        // Priority requested: scripts/ first, then tests/
        out.push_back((std::filesystem::path("scripts") / spec).string());
        if (!has_extension(spec)) out.push_back((std::filesystem::path("scripts") / (spec + ".dts")).string());

        out.push_back((std::filesystem::path("tests") / spec).string());
        if (!has_extension(spec)) out.push_back((std::filesystem::path("tests") / (spec + ".dts")).string());
    }

    // Dedup preserving order
    std::vector<std::string> dedup;
    dedup.reserve(out.size());
    for (const auto& s : out) {
        bool seen = false;
        for (const auto& d : dedup) {
            if (d == s) { seen = true; break; }
        }
        if (!seen) dedup.push_back(s);
    }
    return dedup;
}

static std::optional<std::filesystem::path> resolve_existing_script_path(
    const std::string& spec,
    std::string* attempts_out
) {
    const auto candidates = build_candidate_specs(spec);
    std::ostringstream attempts;

    for (const auto& c : candidates) {
        const auto p = shell_resolve_script_path(c);
        attempts << "  - " << c << " -> " << p.string() << "\n";
        if (std::filesystem::exists(p)) {
            if (attempts_out) *attempts_out = attempts.str();
            return p;
        }
    }

    if (attempts_out) *attempts_out = attempts.str();
    return std::nullopt;
}

static thread_local int g_dotscript_depth = 0;
static bool g_dotscript_trace = false;
static bool g_trace_banner_printed = false;

static void print_usage() {
    std::cout
        << "Usage:\n"
        << "  DOTSCRIPT <file>\n"
        << "  DOTSCRIPT @<file>\n"
        << "  DOTSCRIPT TRACE\n"
        << "  DOTSCRIPT TRACE ON|OFF\n"
        << "  DOTSCRIPT TRACE <file>\n"
        << "  DOTSCRIPT TRACE @<file>\n"
        << "  DOTSCRIPT TRACE ON|OFF <file>\n"
        << "  DOTSCRIPT TRACE ON|OFF @<file>\n";
}

static void maybe_print_trace_banner() {
    if (g_trace_banner_printed) return;
    g_trace_banner_printed = true;

    std::cout
        << "DOTSCRIPT TRACE: resolver search order:\n"
        << "  1) <typed>\n"
        << "  2) <typed>.dts (if no extension)\n"
        << "  3) scripts/<typed>(.dts) (if no parent path)\n"
        << "  4) tests/<typed>(.dts)   (if no parent path)\n";
}

} // namespace

void cmd_DOTSCRIPT(DbArea& area, std::istringstream& args)
{
    std::string rest;
    std::getline(args, rest);
    rest = trim_copy(std::move(rest));

    if (rest.empty()) {
        print_usage();
        return;
    }

    bool trace_for_this_run = g_dotscript_trace;
    std::string file_spec;

    {
        std::istringstream ss(rest);
        std::string t1;
        ss >> t1;

        if (t1.empty()) {
            print_usage();
            return;
        }

        const std::string t1u = upper_copy(t1);

        if (t1u == "TRACE") {
            std::string t2;
            if (!(ss >> t2) || t2.empty()) {
                std::cout << "DOTSCRIPT TRACE is " << (g_dotscript_trace ? "ON" : "OFF") << "\n";
                print_usage();
                return;
            }

            const std::string t2u = upper_copy(t2);

            if (t2u == "ON" || t2u == "OFF") {
                g_dotscript_trace = (t2u == "ON");
                trace_for_this_run = g_dotscript_trace;

                std::string tail;
                std::getline(ss, tail);
                tail = strip_at_prefix(unquote_copy(std::move(tail)));
                if (tail.empty()) {
                    std::cout << "DOTSCRIPT TRACE is now " << (g_dotscript_trace ? "ON" : "OFF") << "\n";
                    return;
                }
                file_spec = tail;
            } else {
                // TRACE <file...> => run once with trace ON; global unchanged.
                trace_for_this_run = true;

                std::string tail;
                std::getline(ss, tail);
                tail = trim_copy(std::move(tail));

                std::string joined = t2;
                if (!tail.empty()) joined += " " + tail;

                file_spec = strip_at_prefix(unquote_copy(std::move(joined)));
            }
        } else {
            // DOTSCRIPT <file...> or DOTSCRIPT @<file...>
            file_spec = strip_at_prefix(unquote_copy(std::move(rest)));
        }
    }

    if (file_spec.empty()) {
        print_usage();
        return;
    }

    // One-level controlled subscript: allow main + 1 subscript; block deeper.
    if (g_dotscript_depth >= 2) {
        std::cout << "DOTSCRIPT: nesting limit reached (max 1 subscript).\n";
        return;
    }

    if (trace_for_this_run) {
        maybe_print_trace_banner();
    }

    std::string attempts;
    const auto resolved = resolve_existing_script_path(file_spec, &attempts);
    if (!resolved) {
        std::cout << "DOTSCRIPT: script not found.\n" << attempts;
        return;
    }

    if (trace_for_this_run) {
        std::cout << "DOTSCRIPT TRACE: resolved: " << resolved->string() << "\n";
    }

    const bool as_subscript = shell_script_active();

    std::string push_err;
    if (!shell_script_push(*resolved, as_subscript, &push_err)) {
        std::cout << "DOTSCRIPT: " << push_err << ": " << resolved->string() << "\n";
        return;
    }

    ScopeExit pop_guard{[] { shell_script_pop(); }};
    ++g_dotscript_depth;
    ScopeExit depth_guard{[] { --g_dotscript_depth; }};

    std::ifstream in(*resolved, std::ios::binary);
    if (!in) {
        std::cout << "DOTSCRIPT: unable to open '" << resolved->string() << "'\n";
        return;
    }

    std::string line;
    size_t lineno = 0;

    while (std::getline(in, line)) {
        ++lineno;

        const std::string trimmed = trim_copy(line);
        if (looks_like_comment_or_blank(trimmed)) continue;

        if (trace_for_this_run) {
            std::cout << resolved->string() << ":" << lineno << "> " << trimmed << "\n";
        }

        if (!shell_execute_line(area, trimmed)) {
            std::cout << "DOTSCRIPT: " << resolved->string() << ":" << lineno
                      << ": Unknown command: " << trimmed << "\n";
        }
    }
}