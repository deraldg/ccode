#include <cctype>
#include <sstream>
#include <string>

#include "cli/expr/ast.hpp"
#include "cli/expr/eval.hpp"
#include "cli/expr/glue_xbase.hpp"
#include "cli/expr/parser.hpp"
#include "predicates.hpp"

namespace {

static bool looks_numeric_literal(const std::string& s) {
    if (s.empty()) return false;
    bool saw_digit = false;
    bool saw_dot = false;
    std::size_t i = 0;

    if (s[i] == '+' || s[i] == '-') ++i;
    for (; i < s.size(); ++i) {
        unsigned char ch = static_cast<unsigned char>(s[i]);
        if (ch >= '0' && ch <= '9') {
            saw_digit = true;
            continue;
        }
        if (ch == '.' && !saw_dot) {
            saw_dot = true;
            continue;
        }
        return false;
    }
    return saw_digit;
}

static std::string upper_copy(std::string s) {
    for (char& c : s) c = static_cast<char>(std::toupper(static_cast<unsigned char>(c)));
    return s;
}

static std::string escape_string_literal(const std::string& s) {
    std::string out;
    out.reserve(s.size() + 8);
    for (char c : s) {
        if (c == '\\' || c == '"') out.push_back('\\');
        out.push_back(c);
    }
    return out;
}

static std::string build_expr(const std::string& fld,
                              const std::string& op,
                              const std::string& val)
{
    std::ostringstream out;
    out << fld << " " << op << " ";

    if (looks_numeric_literal(val)) {
        out << val;
    } else {
        out << "\"" << escape_string_literal(val) << "\"";
    }
    return out.str();
}

} // namespace

namespace predicates {

int field_index_ci(const xbase::DbArea& a, const std::string& name) {
    const auto& f = a.fields();
    const std::string want = upper_copy(name);
    for (size_t i = 0; i < f.size(); ++i) {
        if (upper_copy(f[i].name) == want) return static_cast<int>(i + 1);
    }
    return 0;
}

bool eval(const xbase::DbArea& a,
          const std::string& fld,
          const std::string& op,
          const std::string& val)
{
    if (field_index_ci(a, fld) <= 0) return false;

    const std::string expr_text = build_expr(fld, op, val);

    try {
        dottalk::expr::Parser parser(expr_text);
        auto expr = parser.parse_expr();
        auto rv = dottalk::expr::glue::make_record_view(const_cast<xbase::DbArea&>(a));
        return expr ? expr->eval(rv) : false;
    } catch (...) {
        return false;
    }
}

} // namespace predicates
