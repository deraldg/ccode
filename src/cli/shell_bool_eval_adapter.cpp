#include "shell_bool_eval_adapter.hpp"

#include <cctype>
#include <exception>
#include <iostream>
#include <string>

#include "xbase.hpp"
#include "cli/where_eval_shared.hpp"

// ADD THESE (same pipeline as SCAN)
#include "cli/expr/line_parse_utils.hpp"
#include "cli/expr/normalize_where.hpp"
#include "expr/sql_normalize.hpp"

// These are provided by the shell control-flow module.
extern "C" void while_set_condition_eval(bool(*)(xbase::DbArea&, const std::string&));
extern "C" void until_set_condition_eval(bool(*)(xbase::DbArea&, const std::string&));

namespace {

static ShellAstBoolEval g_injected_eval = nullptr;

static bool quick_literals_ci(const std::string& s, bool& out)
{
    auto up = [](char c) { return char(std::toupper(static_cast<unsigned char>(c))); };

    size_t i = 0;
    size_t j = s.size();
    while (i < j && std::isspace(static_cast<unsigned char>(s[i]))) ++i;
    while (j > i && std::isspace(static_cast<unsigned char>(s[j - 1]))) --j;
    if (i >= j) return false;

    std::string U;
    U.reserve(j - i);
    for (size_t k = i; k < j; ++k) U.push_back(up(s[k]));

    if (U == ".T." || U == "T" || U == "TRUE" || U == "1" || U == "Y") {
        out = true;
        return true;
    }
    if (U == ".F." || U == "F" || U == "FALSE" || U == "0" || U == "N") {
        out = false;
        return true;
    }
    return false;
}

static bool where_boolean_eval(xbase::DbArea& area,
                               const std::string& expr,
                               bool* result,
                               std::string* err)
{
    if (!result) {
        if (err) *err = "null result pointer";
        return false;
    }

    *result = false;

    // --- fast literal path ---
    bool lit = false;
    if (quick_literals_ci(expr, lit)) {
        *result = lit;
        if (err) err->clear();
        return true;
    }

    try {
        using namespace dottalk::expr;

        // --- NEW: normalization pipeline (same as SCAN) ---
        std::string normalized_sql = sqlnorm::sql_to_dottalk_where(expr);
        std::string cleaned        = strip_line_comments(normalized_sql);
        std::string normalized     = normalize_unquoted_rhs_literals(area, cleaned);

        // --- compile using your cached WHERE engine ---
        auto ce = where_eval::compile_where_expr_cached(normalized);

        // --- fallback (important safety net) ---
        if (!ce || !ce->plan) {
            std::string cleaned2    = strip_line_comments(expr);
            std::string normalized2 = normalize_unquoted_rhs_literals(area, cleaned2);
            ce = where_eval::compile_where_expr_cached(normalized2);
        }

        if (!ce || !ce->plan) {
            if (err) *err = "boolean expression compile produced no program";
            return false;
        }

        *result = where_eval::run_program(*ce->plan, area);
        if (err) err->clear();
        return true;

    } catch (const std::exception& e) {
        if (err) *err = e.what();
        return false;
    } catch (...) {
        if (err) *err = "evaluation failed";
        return false;
    }
}

static bool shell_boolean_eval_bridge(xbase::DbArea& area, const std::string& expr)
{
    bool result = false;
    std::string err;

    if (!shell_eval_bool_expr(area, expr, &result, &err)) {
        std::cout << "LOOP CONDITION ERROR: "
                  << (err.empty() ? "evaluation failed" : err) << "\n";
        return false;
    }

    return result;
}

} // namespace

void shell_set_ast_boolean_evaluator(ShellAstBoolEval fn)
{
    g_injected_eval = fn;
}

bool shell_eval_bool_expr(xbase::DbArea& area,
                          const std::string& expr,
                          bool* result,
                          std::string* err)
{
    if (!result) {
        if (err) *err = "null result pointer";
        return false;
    }

    *result = false;

    if (g_injected_eval) {
        return g_injected_eval(area, expr, result, err);
    }

    return where_boolean_eval(area, expr, result, err);
}

void shell_eval_register_for_loops()
{
    while_set_condition_eval(&shell_boolean_eval_bridge);
    until_set_condition_eval(&shell_boolean_eval_bridge);
}