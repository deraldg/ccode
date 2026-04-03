// src/cli/cmd_scan.cpp
// SCAN / ENDSCAN with per-record command execution.
//
// Supports:
//   SCAN [FOR <expr>]
//   ENDSCAN
//
// Notes:
//   - Lines between SCAN..ENDSCAN are buffered via the internal __SCAN_BUFFER command.
//   - Default gate: skips deleted records.
//   - FOR <expr> is compiled by the AST engine (dottalk::expr).
//     SQL-ish input is normalized via sql_to_dottalk_where before compile.
//   - If compile fails, FOR matches no records (safe default).
//   - Honors active SET FILTER via filter::visible(&A, nullptr).
//
// Ordered traversal policy:
//   - No active order: physical recno 1..recCount traversal.
//   - Active order: use shared order_iterator (INX/CNX/CDX).
//   - If ordered traversal fails, fall back to physical order.
//
// Re-entrancy / nesting policy:
//   - Only one SCAN block may be buffered at a time.
//   - Nested SCAN during ENDSCAN execution is not allowed.
//   - Recursive ENDSCAN is not allowed.
//   - Buffering lines during ENDSCAN execution is not allowed.
//
// Execution policy:
//   - SCAN control verbs inside the SCAN body are ignored.
//   - ENDSCAN preserves the user's current cursor position on exit.
//   - Buffered body lines replay through the canonical loop executor so
//     shell shortcuts (e.g. TUP -> TUPLE) work the same as at the prompt.

#include "cmd_scan.hpp"

#include <algorithm>
#include <cctype>
#include <cstdlib>
#include <iostream>
#include <memory>
#include <sstream>
#include <string>
#include <utility>
#include <vector>

#include "xbase.hpp"
#include "textio.hpp"
#include "command_registry.hpp"
#include "scan_state.hpp"
#include "set_relations.hpp"
#include "filters/filter_registry.hpp"

#include "cli/order_state.hpp"
#include "cli/order_nav.hpp"
#include "cli/order_iterator.hpp"

#include "cli/expr/api.hpp"
#include "cli/expr/glue_xbase.hpp"
#include "cli/expr/line_parse_utils.hpp"
#include "cli/expr/normalize_where.hpp"
#include "expr/sql_normalize.hpp"

#include "cmd_loop.hpp"   // loop_get_executor()

using xbase::DbArea;

namespace {

static inline std::string up(std::string s) {
    for (auto& ch : s) ch = static_cast<char>(std::toupper(static_cast<unsigned char>(ch)));
    return s;
}

static inline std::string trim_copy(std::string s) {
    return textio::trim(std::move(s));
}

static inline bool ends_with_iex(const std::string& s, const char* EXT3) {
    const size_t n = s.size();
    return n >= 4 && s[n - 4] == '.'
        && static_cast<char>(std::toupper(static_cast<unsigned char>(s[n - 3]))) == EXT3[0]
        && static_cast<char>(std::toupper(static_cast<unsigned char>(s[n - 2]))) == EXT3[1]
        && static_cast<char>(std::toupper(static_cast<unsigned char>(s[n - 1]))) == EXT3[2];
}

static inline std::string upper_copy(std::string s) {
    for (char& c : s) c = static_cast<char>(std::toupper(static_cast<unsigned char>(c)));
    return s;
}

static inline bool is_comment_or_blank(const std::string& raw) {
    std::string s = textio::trim(raw);
    if (s.empty()) return true;
    if (s[0] == '#') return true;
    if (s.size() >= 2 && s[0] == '/' && s[1] == '/') return true;
    return false;
}

enum class DelGate {
    OnlyAlive,
    OnlyDeleted,
    Any
};

struct EvalProg {
    bool enabled{false};
    std::unique_ptr<dottalk::expr::Expr> prog;

    bool eval(xbase::DbArea& A) const {
        if (!enabled) return true;
        auto rv = dottalk::expr::glue::make_record_view(A);
        return prog->eval(rv);
    }
};

static EvalProg build_evaluator_from(xbase::DbArea& A, const std::string& userExpr) {
    EvalProg E{};

    const std::string normalized_sql = sqlnorm::sql_to_dottalk_where(userExpr);
    const std::string cleaned_sql    = strip_line_comments(normalized_sql);
    const std::string normalized     = normalize_unquoted_rhs_literals(A, cleaned_sql);

    auto cr = dottalk::expr::compile_where(normalized);
    if (!cr) {
        const std::string raw_clean = strip_line_comments(userExpr);
        const std::string normalized2 = normalize_unquoted_rhs_literals(A, raw_clean);
        cr = dottalk::expr::compile_where(normalized2);
    }

    if (!cr) {
        struct AlwaysFalse final : dottalk::expr::Expr {
            bool eval(const dottalk::expr::RecordView&) const override { return false; }
        };
        E.enabled = true;
        E.prog = std::unique_ptr<dottalk::expr::Expr>(new AlwaysFalse{});
        return E;
    }

    E.enabled = true;
    E.prog = std::move(cr.program);
    return E;
}

struct ScanFilter {
    DelGate del{DelGate::OnlyAlive};
    bool haveExpr{false};
    EvalProg E{};

    bool pass_deleted_gate(const xbase::DbArea& a) const {
        bool isDel = false;
        try { isDel = a.isDeleted(); } catch (...) { isDel = false; }

        switch (del) {
            case DelGate::OnlyDeleted: return isDel;
            case DelGate::Any:         return true;
            case DelGate::OnlyAlive:
            default:                   return !isDel;
        }
    }

    bool matches(xbase::DbArea& a) const {
        if (!pass_deleted_gate(a)) return false;
        if (!haveExpr) return true;
        return E.eval(a);
    }
};

struct CursorRestore {
    xbase::DbArea* a{nullptr};
    int32_t saved{0};
    bool active{false};

    explicit CursorRestore(xbase::DbArea& area) : a(&area) {
        try {
            saved = area.recno();
            active = (saved >= 1 && saved <= area.recCount());
        } catch (...) {
            active = false;
        }
    }

    ~CursorRestore() {
        if (!active || !a) return;
        try {
            if (a->gotoRec(saved)) {
                (void)a->readCurrent();
            }
        } catch (...) {
            // best-effort restore only
        }
    }

    CursorRestore(const CursorRestore&) = delete;
    CursorRestore& operator=(const CursorRestore&) = delete;
};

// File-local filter state for the active SCAN.
static ScanFilter g_scan_filter;

// Explicit execution guard for nested/re-entrant SCAN behavior.
static bool g_scan_executing = false;

struct ScanExecGuard {
    ScanExecGuard()  { g_scan_executing = true;  }
    ~ScanExecGuard() { g_scan_executing = false; }

    ScanExecGuard(const ScanExecGuard&) = delete;
    ScanExecGuard& operator=(const ScanExecGuard&) = delete;
};

static ScanFilter compile_filter(DbArea& A, std::istringstream& iss, bool& hadFor) {
    ScanFilter f{};
    hadFor = false;

    std::streampos save = iss.tellg();
    std::string tok;
    if (!(iss >> tok)) {
        return f;
    }

    if (!textio::ieq(tok, "FOR")) {
        iss.clear();
        iss.seekg(save);
        return f;
    }

    hadFor = true;

    std::string rest;
    std::getline(iss, rest);
    rest = trim_copy(rest);

    if (rest.empty()) {
        return f;
    }

    {
        const std::string U = up(rest);
        if (U == "DELETED") {
            f.del = DelGate::OnlyDeleted;
            f.haveExpr = false;
            return f;
        }
        if (U == "!DELETED" || U == "~DELETED") {
            f.del = DelGate::OnlyAlive;
            f.haveExpr = false;
            return f;
        }
    }

    f.haveExpr = true;
    const std::string norm = normalize_unquoted_rhs_literals(A, rest);
    f.E = build_evaluator_from(A, norm);
    return f;
}

static bool is_scan_control_verb(const std::string& cmdU) {
    return cmdU == "SCAN" || cmdU == "ENDSCAN" || cmdU == "__SCAN_BUFFER";
}

static std::vector<std::string> sanitize_body_lines(const std::vector<std::string>& lines, int& skipped_controls) {
    std::vector<std::string> out;
    skipped_controls = 0;

    out.reserve(lines.size());
    for (const auto& raw : lines) {
        const std::string trimmed = textio::trim(raw);
        if (trimmed.empty()) continue;

        std::istringstream li(trimmed);
        std::string cmd;
        li >> cmd;
        if (cmd.empty()) continue;

        const std::string U = up(cmd);
        if (is_scan_control_verb(U)) {
            ++skipped_controls;
            continue;
        }

        out.push_back(raw);
    }

    return out;
}

static void exec_lines(DbArea& A, const std::vector<std::string>& lines) {
    auto* exec = loop_get_executor();

    for (const auto& raw : lines) {
        const std::string trimmed = textio::trim(raw);
        if (trimmed.empty()) continue;

        std::istringstream li(trimmed);
        std::string cmd;
        li >> cmd;
        if (cmd.empty()) continue;

        const std::string U = up(cmd);
        if (is_scan_control_verb(U)) {
            continue;
        }

        if (exec) {
            (*exec)(A, trimmed);
        } else if (!dli::registry().run(A, U, li)) {
            std::cout << "Unknown command in SCAN: " << cmd << "\n";
        }
    }
}

static void run_scan_on_recno(DbArea& A,
                              uint64_t rn,
                              const std::vector<std::string>& lines,
                              const ScanFilter& filter,
                              int& matched,
                              int& iterations) {
    try {
        if (!A.gotoRec(static_cast<int32_t>(rn)) || !A.readCurrent()) return;
    } catch (...) {
        return;
    }

    relations_api::refresh_if_enabled();

    bool ok = false;
    try { ok = filter.matches(A) && filter::visible(&A, nullptr); } catch (...) { ok = false; }
    if (!ok) return;

    ++matched;
    exec_lines(A, lines);
    ++iterations;
    std::cout.flush();
}

static void run_scan_physical(DbArea& A,
                              const std::vector<std::string>& lines,
                              const ScanFilter& filter,
                              int nrecs,
                              int& matched,
                              int& iterations) {
    if (nrecs <= 0) return;

    for (int32_t rn = 1; rn <= nrecs; ++rn) {
        run_scan_on_recno(A, static_cast<uint64_t>(rn), lines, filter, matched, iterations);
    }
}

static bool run_scan_via_iterator(DbArea& A,
                                  const std::vector<std::string>& lines,
                                  const ScanFilter& filter,
                                  int& matched,
                                  int& iterations) {
    cli::OrderIterSpec spec{};
    std::string err;

    const bool ok = cli::order_iterate_recnos(
        A,
        [&](uint64_t rn) -> bool {
            run_scan_on_recno(A, rn, lines, filter, matched, iterations);
            return true;
        },
        &spec,
        &err
    );

    (void)spec;
    (void)err;
    return ok;
}

} // namespace

void cmd_SCAN(DbArea& A, std::istringstream& S)
{
    auto& st = scanblock::state();

    if (g_scan_executing) {
        std::cout << "SCAN: nested SCAN not allowed during ENDSCAN.\n";
        return;
    }

    if (st.active) {
        std::cout << "SCAN: already buffering lines. Type ENDSCAN to execute.\n";
        return;
    }

    if (!A.isOpen()) {
        st.active = false;
        st.lines.clear();
        std::cout << "SCAN: no table open in current area.\n";
        return;
    }

    st.active = true;
    st.lines.clear();

    bool hadFor = false;
    g_scan_filter = compile_filter(A, S, hadFor);

    std::cout << "SCAN: buffering lines. Type ENDSCAN to execute"
              << (hadFor ? " (FOR present)." : ".") << "\n";
}

void cmd_SCAN_BUFFER(DbArea& /*A*/, std::istringstream& S)
{
    auto& st = scanblock::state();

    if (g_scan_executing) {
        std::cout << "SCAN: cannot buffer lines during ENDSCAN.\n";
        return;
    }

    if (!st.active) return;

    std::string raw;
    std::getline(S >> std::ws, raw);
    if (raw.empty()) return;
    if (is_comment_or_blank(raw)) return;

    st.lines.push_back(raw);
}

void cmd_ENDSCAN(DbArea& A, std::istringstream& /*S*/)
{
    auto& st = scanblock::state();

    if (g_scan_executing) {
        std::cout << "ENDSCAN: already executing.\n";
        return;
    }

    if (!st.active) {
        std::cout << "No active SCAN.\n";
        return;
    }

    CursorRestore restore(A);

    std::vector<std::string> raw_lines = st.lines;
    ScanFilter filter = std::move(g_scan_filter);

    st.active = false;
    st.lines.clear();
    g_scan_filter = ScanFilter{};

    int skipped_controls = 0;
    std::vector<std::string> lines = sanitize_body_lines(raw_lines, skipped_controls);
    const int buffered = static_cast<int>(lines.size());

    if (skipped_controls > 0) {
        std::cout << "SCAN: ignored " << skipped_controls
                  << " control line(s) inside SCAN body.\n";
    }

    int nrecs = 0;
    try { nrecs = A.recCount(); } catch (...) { nrecs = 0; }

    int matched = 0;
    int iterations = 0;

    {
        ScanExecGuard exec_guard;

        if (orderstate::hasOrder(A)) {
            if (!run_scan_via_iterator(A, lines, filter, matched, iterations)) {
                run_scan_physical(A, lines, filter, nrecs, matched, iterations);
            }
        } else {
            run_scan_physical(A, lines, filter, nrecs, matched, iterations);
        }
    }

    std::cout << "ENDSCAN: " << matched
              << " match(es), " << iterations
              << " iteration(s) over " << buffered
              << " line(s).\n";
}