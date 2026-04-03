// src/where_hook_stub.cpp
// Stub evaluator + field getter. Replace when wiring real engine.

#include <string>
#include <algorithm>
#include <cctype>
#include <tuple>
#include "xbase.hpp"
#include "where_hook.hpp"

namespace {

static inline std::string trim(std::string s) {
    auto not_space = [](int ch){ return !std::isspace(static_cast<unsigned char>(ch)); };
    s.erase(s.begin(), std::find_if(s.begin(), s.end(), not_space));
    s.erase(std::find_if(s.rbegin(), s.rend(), not_space).base(), s.end());
    return s;
}

static inline std::string unquote(std::string s) {
    s = trim(std::move(s));
    if (s.size() >= 2 && ((s.front()=='"'&&s.back()=='"')||(s.front()=='\''&&s.back()=='\''))) {
        s = s.substr(1, s.size()-2);
    }
    return s;
}

static inline std::string to_upper(std::string s) {
    for (auto& c : s) c = static_cast<char>(std::toupper(static_cast<unsigned char>(c)));
    return s;
}

static std::tuple<std::string,std::string,std::string> parse_expr(const std::string& expr_raw) {
    // Returns (field, op, value). Supports =, ==, != ; tolerant spacing; field can be bare.
    std::string expr = expr_raw;
    // Normalize spaces around operators to make splitting simpler.
    auto pos = expr.find("!=");
    std::string op;
    if (pos != std::string::npos) {
        op = "!=";
    } else if ((pos = expr.find("==")) != std::string::npos) {
        op = "==";
    } else if ((pos = expr.find('=')) != std::string::npos) {
        op = "=";
    } else {
        // No operator ? treat whole as value against a magic TRUE constant.
        return {"", "", trim(expr)};
    }

    std::string lhs = trim(expr.substr(0, pos));
    std::string rhs = trim(expr.substr(pos + op.size()));
    rhs = unquote(rhs);
    return {lhs, op, rhs};
}

} // namespace

namespace wherehook {

// Default field getter stub: returns false (unavailable).
static bool default_get_field(xbase::DbArea&, const std::string&, std::string&) {
    return false;
}

// Wire points (can be overridden by real engine TUs)
FieldGetter get_field = &default_get_field;

static bool stub_eval(xbase::DbArea& area, const std::string& expr, DebugInfo* dbg)
{
    auto [field, op, rhs] = parse_expr(expr);
    // Special-case: "1 = 1" etc.
    if (field.empty() && op.empty()) {
        if (dbg) { dbg->cli_value_searched = rhs; dbg->field_value_examined = "N/A"; }
        return to_upper(trim(rhs)) == "TRUE" || rhs == "1";
    }
    if (to_upper(trim(field)) == "1" && (op == "=" || op == "==")) {
        if (dbg) { dbg->cli_value_searched = "1"; dbg->field_value_examined = "1"; }
        return rhs == "1";
    }

    std::string field_val;
    bool have_field = get_field && get_field(area, field, field_val);

    if (dbg) {
        dbg->cli_value_searched   = rhs;
        dbg->field_value_examined = have_field ? field_val : "(unavailable)";
    }

    if (!have_field) {
        // Without a real getter, fail closed (comparison false).
        if (op == "!=") return true; // conservative: unknown != rhs is ?true? often useful for DEBUG
        return false;
    }

    // Equality / inequality; case-sensitive by default.
    if (op == "!=") return field_val != rhs;
    /* "=", "==" or anything else fall back to equality */
    return field_val == rhs;
}

// Expose stub evaluator by default; remove when wiring real evaluator.
EvalFn evaluator = &stub_eval;

} // namespace wherehook



