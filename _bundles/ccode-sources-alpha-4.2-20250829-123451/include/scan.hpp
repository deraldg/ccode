#pragma once
#include <algorithm>
#include <cctype>
#include <optional>
#include <string>
#include "xbase.hpp"
#include "scan_options.hpp"
#include "predicates.hpp"  // header-only: provides predicates::eval(...)

struct ScanStats { int visited=0, tested=0, matched=0, acted=0; };

// Parse "FIELD OP VALUE" and evaluate via predicates::eval.
// VALUE may be quoted; OP is case-insensitive. (predicates::eval handles unquote)
inline bool eval_cond_inline(const xbase::DbArea& A, const std::string& expr) {
    size_t i = 0, n = expr.size();
    auto ws = [&](char c){ return std::isspace(static_cast<unsigned char>(c)); };
    auto skip = [&]{ while (i < n && ws(expr[i])) ++i; };

    skip(); size_t b = i; while (i < n && !ws(expr[i])) ++i; std::string fld = expr.substr(b, i - b);
    skip(); b = i;        while (i < n && !ws(expr[i])) ++i; std::string op  = expr.substr(b, i - b);
    skip();               std::string val = (i < n) ? expr.substr(i) : std::string();

    if (fld.empty() || op.empty() || val.empty()) return false;

    for (char& c : op) c = static_cast<char>(std::toupper(static_cast<unsigned char>(c)));
    return predicates::eval(A, fld, op, val);
}

template <class Action>
ScanStats scan_records(xbase::DbArea& A, const ScanOptions& opt, Action per_record) {
    ScanStats st{};
    if (!A.isOpen() || A.recCount() <= 0) return st;

    const int nrecs = A.recCount();

    auto passes_del = [&](bool isDeleted)->bool {
        switch (opt.del_mode) {
            case ScanOptions::DeleteMode::SkipDeleted:    return !isDeleted;
            case ScanOptions::DeleteMode::IncludeDeleted: return true;
            case ScanOptions::DeleteMode::OnlyDeleted:    return isDeleted;
        }
        return true;
    };

    auto cond_ok = [&](const std::optional<std::string>& expr)->bool {
        if (!expr) return true;
        return eval_cond_inline(A, *expr);
    };

    int start = A.recno();
    if (start < 1 || start > nrecs) start = 1;

    if (opt.range == ScanOptions::Range::RecordN) {
        if (opt.n < 1 || opt.n > nrecs) return st;
        if (!A.gotoRec(opt.n)) return st;
        st.visited++; (void)A.readCurrent();
        bool isDel = false; // TODO: wire actual deleted flag if/when exposed
        if (passes_del(isDel) && cond_ok(opt.while_expr) && cond_ok(opt.for_expr)) {
            st.tested++; st.matched++;
            if (per_record(A)) st.acted++;
        }
        return st;
    }

    int idx = start;
    int max_steps =
        (opt.range == ScanOptions::Range::NextN) ? std::max(0, opt.n) :
        (opt.range == ScanOptions::Range::Rest)  ? (nrecs - start + 1) :
                                                   nrecs;

    auto advance = [&](int& r){ if (++r > nrecs) r = 1; };

    if (!A.gotoRec(idx)) return st;

    int steps = 0;
    bool wrapped_once = false;
    while (steps < max_steps) {
        st.visited++; (void)A.readCurrent();
        bool isDel = false; // TODO

        if (passes_del(isDel)) {
            st.tested++;
            if (!cond_ok(opt.while_expr)) break;
            if (cond_ok(opt.for_expr)) {
                st.matched++;
                if (!per_record(A)) break;
                st.acted++;
            }
        }

        steps++;
        if (opt.range == ScanOptions::Range::AllFromCurrent) {
            advance(idx);
            if (!A.gotoRec(idx)) break;
            if (idx == start) {
                if (wrapped_once) break;
                wrapped_once = true;
            }
        } else {
            if (idx == nrecs) break;
            advance(idx);
            if (!A.gotoRec(idx)) break;
        }
    }
    return st;
}
