// src/cli/cmd_count.cpp
#include "xbase.hpp"
#include "xbase_field_getters.hpp"         // standard field lookup/getters
#include "record_view.hpp"
#include "textio.hpp"
#include "predicate_eval.hpp"              // predx::eval_expr

#include <iostream>
#include <sstream>
#include <string>
#include <cctype>
#include <algorithm>
#include <cstdlib>
#include <memory>

// DotTalk expr (for complex booleans / parentheses)
#include "dottalk/expr/api.hpp"
#include "dottalk/expr/for_parser.hpp"

// Map DbArea getters to evaluator glue -> use xfg helpers
#define DOTTALK_GET_FIELD_STR(area, name)  xfg::getFieldAsString(area, name)
#define DOTTALK_GET_FIELD_NUM(area, name)  xfg::getFieldAsNumber(area, name)
#include "dottalk/expr/glue_xbase.hpp"

// ---------------------------------------------------------------------------
// Your implementations of these live at *global scope* in src/cli/expr/*.cpp.
// Declare them here so we can call ::extract_for_clause / ::compile_where.
namespace dottalk { namespace expr { struct CompileResult; } }
bool extract_for_clause(std::istringstream& iss, std::string& out);
dottalk::expr::CompileResult compile_where(const std::string& text);
// ---------------------------------------------------------------------------

namespace {

// trim helper
static inline std::string trim(std::string s) {
    auto sp = [](unsigned char c){ return c==' '||c=='\t'||c=='\r'||c=='\n'; };
    while (!s.empty() && sp((unsigned char)s.front())) s.erase(s.begin());
    while (!s.empty() && sp((unsigned char)s.back()))  s.pop_back();
    return s;
}

// simple upper
static inline std::string up(std::string s) {
    std::transform(s.begin(), s.end(), s.begin(),
                   [](unsigned char c){ return (char)std::toupper(c); });
    return s;
}

// Heuristic for complex WHERE (Boolean ops/parens) — use DotTalk when true
static bool looks_complex_where(const std::string& s) {
    auto U = up(trim(s));
    return U.find(" AND ") != std::string::npos
        || U.find(" OR ")  != std::string::npos
        || U.find(".AND.") != std::string::npos
        || U.find(".OR.")  != std::string::npos
        || U.find(".NOT.") != std::string::npos
        || U.find('(')     != std::string::npos
        || U.find(')')     != std::string::npos;
}

enum class DelMode { SkipDeleted, OnlyDeleted, IncludeAll };

struct Opts {
    DelMode mode = DelMode::SkipDeleted; // COUNT (default) => skip deleted
    bool    haveFor = false;
    std::string forRaw;  // whatever remains after FOR
    bool    haveTriplet = false;
    std::string fld, op, val;
};

// Very small parser for: COUNT [ALL|DELETED] [FOR ...]
static Opts parse_opts(std::istringstream& iss) {
    Opts o;
    // Rebuild tail from the stream and then parse it.
    std::string rest;
    {
        const std::string& all = iss.str();
        auto pos = iss.tellg();
        if (pos != std::istringstream::pos_type(-1)) {
            size_t i = static_cast<size_t>(pos);
            if (i < all.size()) rest = all.substr(i);
        } else rest = all;
    }
    rest = trim(rest);

    // Leading ALL/DELETED?
    std::istringstream head(rest);
    std::string t1;
    std::streampos afterT1 = head.tellg();
    if (head >> t1) {
        auto T1 = up(t1);
        if (T1 == "ALL")       { o.mode = DelMode::IncludeAll; }
        else if (T1 == "DELETED") { o.mode = DelMode::OnlyDeleted; }
        else {
            // not a mode; rewind
            head.clear();
            head.seekg(afterT1);
        }
    }

    // Remaining string after optional mode
    std::string tail; std::getline(head, tail);
    tail = trim(tail);

    // Detect FOR
    if (!tail.empty()) {
        auto U = up(tail);
        if (U.rfind("FOR", 0) == 0) {
            o.haveFor = true;
            o.forRaw = trim(tail.substr(3));
            // cheap triplet probe: "<token> <op> <rest>"
            std::istringstream ts(o.forRaw);
            std::string lhs, op;
            if ((ts >> lhs) && (ts >> op)) {
                o.fld = lhs; o.op = op;
                std::string rest2; std::getline(ts, rest2);
                o.val = trim(rest2);
                o.haveTriplet = !o.val.empty();
            }
        }
    }
    return o;
}

} // anon

void cmd_COUNT(xbase::DbArea& A, std::istringstream& iss) {
    if (!A.isOpen()) { std::cout << "No file open\n"; return; }

    const Opts opt = parse_opts(iss);

    auto include_row = [&](bool deleted)->bool {
        if (opt.mode == DelMode::SkipDeleted && deleted) return false;
        if (opt.mode == DelMode::OnlyDeleted && !deleted) return false;
        return true;
    };

    // Prepare predicate: predx fast path for simple cases, DotTalk for complex boolean, or none.
    std::unique_ptr<dottalk::expr::Expr> prog;
    std::string expr_line; // for predx
    bool use_dottalk = false;

    if (opt.haveFor) {
        use_dottalk = looks_complex_where(opt.forRaw);
        if (use_dottalk) {
            // NOTE: Call the *global* function; your implementation is at global scope.
            auto cr = ::compile_where(opt.forRaw);
            if (!cr) {
                std::cout << "Syntax error in FOR: " << cr.error << "\n";
                return;
            }
            prog = std::move(cr.program);
        } else {
            // predx path for simple triplet
            expr_line = "FOR " + opt.forRaw;
        }
    }

    long long cnt = 0;

    if (A.top() && A.readCurrent()) {
        do {
            if (!include_row(A.isDeleted())) continue;

            if (!opt.haveFor) {
                ++cnt;     // plain COUNT
            } else if (prog) {
                auto rv = dottalk::expr::glue::make_record_view(A);
                if (prog->eval(rv)) ++cnt;
            } else {
                // predx::eval_expr handles FOR triplet exactly as your current codepath.
                if (predx::eval_expr(A, expr_line)) ++cnt;
            }
        } while (A.skip(+1) && A.readCurrent());
    }

    std::cout << cnt << "\n";
}
