// ============================================================================
// path: src/cli/cmd_loop.cpp
// purpose: LOOP implementation with numeric repetition (N TIMES)
// notes  :
//   - LOOP
//       buffer + replay once
//   - LOOP <n>   | LOOP <n> TIMES
//       buffer + replay n times
//   - LOOP FOR <n> [TIMES]
//       same as above (compat with "FOR" phrasing)
//   - LOOP FOR <expr>
//       currently acts as a label only if not numeric
//   - hard default max iterations: 1000
// ============================================================================
#include "loop_state.hpp"
#include "cmd_loop.hpp"

#include <algorithm>
#include <cctype>
#include <iostream>
#include <limits>
#include <optional>
#include <sstream>
#include <string>

namespace {

static LoopExecFn g_loop_exec = nullptr;
static constexpr size_t kDefaultMaxLoopIterations = 100000000;

static inline void upcase_ascii(std::string& s)
{
    std::transform(
        s.begin(), s.end(), s.begin(),
        [](unsigned char c) { return static_cast<char>(std::toupper(c)); });
}

static inline std::string trim_ascii(std::string s)
{
    auto issp = [](unsigned char c) {
        return c == ' ' || c == '\t' || c == '\r' || c == '\n';
    };

    while (!s.empty() && issp(static_cast<unsigned char>(s.front()))) {
        s.erase(s.begin());
    }
    while (!s.empty() && issp(static_cast<unsigned char>(s.back()))) {
        s.pop_back();
    }
    return s;
}

static inline bool starts_with_ascii(const std::string& s, const std::string& prefix)
{
    if (prefix.size() > s.size()) return false;
    for (size_t i = 0; i < prefix.size(); ++i) {
        if (static_cast<unsigned char>(s[i]) != static_cast<unsigned char>(prefix[i])) {
            return false;
        }
    }
    return true;
}

// Try to parse "<n> [TIMES]" from free-form text.
// Returns {true, n} if a leading integer exists.
static std::pair<bool, size_t> parse_leading_count(const std::string& s0)
{
    std::string s = trim_ascii(s0);
    if (s.empty()) return {false, 0};

    size_t i = 0;
    if (s[i] == '+' || s[i] == '-') ++i;
    if (i >= s.size() || !std::isdigit(static_cast<unsigned char>(s[i]))) {
        return {false, 0};
    }

    long long val = 0;
    while (i < s.size() && std::isdigit(static_cast<unsigned char>(s[i]))) {
        int d = s[i] - '0';
        if (val > (std::numeric_limits<long long>::max() - d) / 10) {
            return {false, 0};
        }
        val = val * 10 + d;
        ++i;
    }

    while (i < s.size() && (s[i] == ' ' || s[i] == '\t')) ++i;

    if (val <= 0) return {true, 0};
    return {true, static_cast<size_t>(val)};
}

// Parse LOOP flags/args.
// Supports:
//   LOOP
//   LOOP QUIET
//   LOOP <n>
//   LOOP <n> TIMES
//   LOOP FOR <n>
//   LOOP FOR <n> TIMES OVERRIDE (Developer)
static void parse_loop_flags(std::istringstream& S, bool& quiet, std::string& label)
{
    quiet = false;
    label.clear();

    std::string tok;
    if (!(S >> tok)) return;

    std::string U = tok;
    upcase_ascii(U);

    if (U == "QUIET") {
        quiet = true;
        return;
    }

    if (U == "FOR") {
        std::string rest;
        std::getline(S, rest);
        label = trim_ascii(rest);
        return;
    }

    if (U == "OVERRIDE") {
        std::string rest;
        std::getline(S, rest);
        label = trim_ascii(rest);
        return;
    }

    std::string rest;
    std::getline(S, rest);
    label = trim_ascii(tok + rest);
}

static size_t iterations_from_label(const std::string& label)
{
    auto [ok, n] = parse_leading_count(label);
    if (!ok) return 1;
    return n;
}

} // namespace

// ---- public API -------------------------------------------------------------

void loop_set_executor(LoopExecFn fn)
{
    g_loop_exec = fn;
}

LoopExecFn loop_get_executor()
{
    return g_loop_exec;
}

// ---- commands ---------------------------------------------------------------

void cmd_LOOP(xbase::DbArea&, std::istringstream& S)
{
    auto& st = loopblock::state();
    st.lines.clear();

    bool quiet = false;
    std::string label;
    parse_loop_flags(S, quiet, label);

    st.active = true;
    st.quiet = quiet;
    st.for_expr = label.empty()
        ? std::optional<std::string>{}
        : std::optional<std::string>{label};

    if (!st.quiet) {
        std::cout << "LOOP: buffering commands";
        if (st.for_expr) {
            std::cout << " FOR " << *st.for_expr;
        }
        std::cout << ". Type ENDLOOP to execute.\n";
    }
}

void cmd_LOOP_BUFFER(xbase::DbArea&, std::istringstream& S)
{
    auto& st = loopblock::state();

    std::string raw;
    std::getline(S, raw);
    raw = trim_ascii(raw);

    if (!raw.empty()) {
        st.lines.push_back(raw);
    }
}

void cmd_ENDLOOP(xbase::DbArea& A, std::istringstream&)
{
    auto& st = loopblock::state();
    if (!st.active) {
        std::cout << "ENDLOOP: not in a LOOP.\n";
        return;
    }

    st.active = false;

    const size_t buffered = st.lines.size();

    size_t iters_requested = 1;
    if (st.for_expr && !st.for_expr->empty()) {
        iters_requested = iterations_from_label(*st.for_expr);
    }

    if (iters_requested > kDefaultMaxLoopIterations) {
        if (!st.quiet) {
            std::cout << "ENDLOOP: iteration count " << iters_requested
                      << " exceeds max " << kDefaultMaxLoopIterations
                      << "; clamping.\n";
        }
        iters_requested = kDefaultMaxLoopIterations;
    }

    size_t iters_executed = 0;

    if (g_loop_exec) {
        for (; iters_executed < iters_requested; ++iters_executed) {
            for (const std::string& raw : st.lines) {
                std::string t = trim_ascii(raw);
                if (t.empty()) continue;

                std::string up = t;
                upcase_ascii(up);
                if (starts_with_ascii(up, "ENDLOOP")) continue;

                g_loop_exec(A, t);
            }
        }
    } else if (!st.quiet) {
        std::cout << "LOOP: no executor; buffered lines not executed\n";
    }

    if (!st.quiet) {
        std::cout << "ENDLOOP: " << buffered << " buffered line(s)";
        if (st.for_expr && !st.for_expr->empty()) {
            std::cout << ", FOR " << *st.for_expr;
        }
        if (iters_requested != 1) {
            std::cout << " -> " << iters_executed << " iteration(s)";
        }
        std::cout << "\n";
    }

    // Next LOOP clears st.lines.
}