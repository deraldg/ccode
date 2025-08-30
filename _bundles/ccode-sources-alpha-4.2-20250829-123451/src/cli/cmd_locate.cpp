// src/cli/cmd_locate.cpp
// LOCATE [FOR <expr>]   (boolean ops + parentheses supported)
// Also accepts: LOCATE <field> <op> <value>  (shorthand; we compile it)

#include <iostream>
#include <sstream>
#include <string>
#include <memory>
#include <cctype>
#include <utility>

#include "xbase.hpp"
#include "textio.hpp"
#include "predicate_eval.hpp"   // predx::eval_expr for simple triplets
#include "xbase_field_getters.hpp"  // xfg helpers

// DotTalk++ expression engine
#include "dottalk/expr/api.hpp"
#include "dottalk/expr/for_parser.hpp"

// Map DbArea getters to evaluator glue -> use xfg helpers
#define DOTTALK_GET_FIELD_STR(area, name)  xfg::getFieldAsString(area, name)
#define DOTTALK_GET_FIELD_NUM(area, name)  xfg::getFieldAsNumber(area, name)
#include "dottalk/expr/glue_xbase.hpp"

// ---------------------------------------------------------------------------
// Forward declarations for the GLOBAL implementations you have in src/cli/expr
namespace dottalk { namespace expr { struct CompileResult; } }
bool extract_for_clause(std::istringstream& iss, std::string& out);
dottalk::expr::CompileResult compile_where(const std::string& text);
// ---------------------------------------------------------------------------

// Trim both ends (ASCII)
static std::string trim(std::string s) {
    auto issp = [](unsigned char c){ return std::isspace(c)!=0; };
    while (!s.empty() && issp((unsigned char)s.front())) s.erase(s.begin());
    while (!s.empty() && issp((unsigned char)s.back()))  s.pop_back();
    return s;
}

// Simple heuristic to decide if expression likely needs full boolean parser
static bool looks_complex_where(const std::string& s) {
    std::string U = s;
    for (auto& c : U) c = (char)std::toupper((unsigned char)c);
    return U.find(" AND ") != std::string::npos
        || U.find(" OR ")  != std::string::npos
        || U.find(".AND.") != std::string::npos
        || U.find(".OR.")  != std::string::npos
        || U.find(".NOT.") != std::string::npos
        || U.find('(')     != std::string::npos
        || U.find(')')     != std::string::npos;
}

void cmd_LOCATE(xbase::DbArea& A, std::istringstream& iss) {
    if (!A.isOpen()) { std::cout << "No table open.\n"; return; }

    // Try explicit: LOCATE ... FOR <expr>
    std::string where_text;
    bool has_where = ::extract_for_clause(iss, where_text); // GLOBAL symbol in your build

    // If no explicit FOR was found, accept "LOCATE <field> <op> <value>".
    std::string rest;
    if (!has_where) {
        std::ostringstream os; os << iss.rdbuf();
        rest = trim(os.str());
        if (rest.empty()) {
            std::cout << "Syntax: LOCATE [FOR <expr>]  or  LOCATE <field> <op> <value>\n";
            return;
        }
        where_text = rest;  // compile as-is
        has_where  = true;
    }

    // Decide engine
    const bool use_dottalk = looks_complex_where(where_text) || (where_text.find('(') != std::string::npos);

    // Compile predicate (DotTalk path)
    std::unique_ptr<dottalk::expr::Expr> prog;
    if (use_dottalk) {
        auto cr = ::compile_where(where_text); // GLOBAL symbol in your build
        if (!cr) {
            std::cout << "Syntax error in FOR: " << cr.error << "\n";
            return;
        }
        prog = std::move(cr.program);
    }

    // Scan from TOP; stop at first match
    if (!A.top()) { std::cout << "Not Located.\n"; return; }

    do {
        if (!A.readCurrent()) continue;
        if (A.isDeleted()) continue; // honor deleted filter

        bool match = false;
        if (use_dottalk && prog) {
            auto rv = dottalk::expr::glue::make_record_view(A);
            match = prog->eval(rv);
        } else {
            // Keep predx support for bare triplets if you still want it.
            // Accept both "FOR fld op val" and "fld op val"
            std::string trip = where_text;
            std::string U = where_text;
            for (auto& c : U) c = (char)std::toupper((unsigned char)c);
            if (U.rfind("FOR", 0) != 0) trip = "FOR " + where_text;
            match = predx::eval_expr(A, trip);
        }

        if (match) {
            std::cout << "Located.\n";
            return;
        }
    } while (A.skip(1));

    std::cout << "Not Located.\n";
}
