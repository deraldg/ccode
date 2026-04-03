#include "predicate_eval.hpp"
#include "textio.hpp"
#include "record_view.hpp"
#include "xbase.hpp"

#include <algorithm>
#include <cctype>
#include <cstdlib>
#include <sstream>
#include <string>

// ================= Global state =================
static bool g_case_sensitive = false;
void predx::set_case_sensitive(bool on) { g_case_sensitive = on; }
bool predx::get_case_sensitive() { return g_case_sensitive; }

// ================= Helpers ======================
static inline std::string trim(std::string s) {
    auto sp = [](unsigned char c){ return c==' '||c=='\t'||c=='\r'||c=='\n'; };
    while (!s.empty() && sp((unsigned char)s.front())) s.erase(s.begin());
    while (!s.empty() && sp((unsigned char)s.back()))  s.pop_back();
    return s;
}
static inline std::string dequote(std::string s) {
    s = trim(std::move(s));
    if (s.size() >= 2) {
        char a = s.front(), b = s.back();
        if ((a=='"' && b=='"') || (a=='\'' && b=='\'')) return s.substr(1, s.size()-2);
    }
    return s;
}
static inline std::string upper(std::string s) {
    for (auto& ch : s) ch = (char)std::toupper((unsigned char)ch);
    return s;
}
static inline std::string lower(std::string s) {
    for (auto& ch : s) ch = (char)std::tolower((unsigned char)ch);
    return s;
}
static inline bool ieq(const std::string& a, const std::string& b) {
    if (a.size()!=b.size()) return false;
    for (size_t i=0;i<a.size();++i)
        if (std::toupper((unsigned char)a[i]) != std::toupper((unsigned char)b[i])) return false;
    return true;
}
static inline bool parse_double(const std::string& s, double& out) {
    if (s.empty()) return false;
    char* end=nullptr; out = std::strtod(s.c_str(), &end);
    return end && *end=='\0';
}
static inline bool is_bool_true(std::string s) {
    s = upper(trim(std::move(s)));
    return (s=="T" || s==".T." || s=="TRUE" || s=="1");
}
static inline bool is_bool_false(std::string s) {
    s = upper(trim(std::move(s)));
    return (s=="F" || s==".F." || s=="FALSE" || s=="0");
}

// resolve field index by case-insensitive name
static int resolve_field_index(xbase::DbArea& db, const std::string& nameIn) {
    const auto name = upper(trim(nameIn));
    const auto F = db.fields();
    for (int i = 0; i < (int)F.size(); ++i) {
        if (upper(trim(F[i].name)) == name) return i;
    }
    return -1;
}

// read current record's value for field index as a trimmed string
static std::string get_cur_string(xbase::DbArea& db, int fi) {
    RecordView rv(db);
    return textio::rtrim(rv.getString(fi));
}

// comparisons
static bool cmp_string(std::string lhs, std::string op, std::string rhs) {
    op = upper(std::move(op));
    if (!g_case_sensitive) { lhs = upper(lhs); rhs = upper(rhs); }
    if (op=="=")  return lhs == rhs;
    if (op=="!=") return lhs != rhs;
    if (op=="$")  return lhs.find(rhs) != std::string::npos;  // contains
    return false;
}
static bool cmp_number(double lhs, std::string op, double rhs) {
    op = upper(std::move(op));
    if (op=="=")  return lhs == rhs;
    if (op=="!=") return lhs != rhs;
    if (op==">")  return lhs >  rhs;
    if (op==">=") return lhs >= rhs;
    if (op=="<")  return lhs <  rhs;
    if (op=="<=") return lhs <= rhs;
    return false;
}
static bool cmp_bool(bool lhs, std::string op, bool rhs) {
    op = upper(std::move(op));
    if (op=="=")  return lhs == rhs;
    if (op=="!=") return lhs != rhs;
    return false;
}

// constants: ".T.", "TRUE", "1", etc.
static bool try_const_bool(std::string s, bool& out) {
    s = upper(trim(dequote(std::move(s))));
    if (is_bool_true(s))  { out = true;  return true; }
    if (is_bool_false(s)) { out = false; return true; }
    return false;
}

// If both sides are literal numbers/bools, evaluate without fields.
static bool try_const_compare(std::string lhsRaw, std::string op, std::string rhsRaw, bool& out) {
    const bool lhsIsB = is_bool_true(lhsRaw) || is_bool_false(lhsRaw);
    const bool rhsIsB = is_bool_true(rhsRaw) || is_bool_false(rhsRaw);
    if (lhsIsB && rhsIsB) {
        bool lhs = is_bool_true(lhsRaw), rhs = is_bool_true(rhsRaw);
        out = cmp_bool(lhs, op, rhs);
        return true;
    }
    double ln=0, rn=0;
    if (parse_double(lhsRaw, ln) && parse_double(rhsRaw, rn)) {
        out = cmp_number(ln, op, rn);
        return true;
    }
    return false;
}

// Recognize UPPER(name) / LOWER(name)
enum class Fn { None, Upper, Lower };
static bool parse_functor_token(std::string token, Fn& fn, std::string& inner) {
    token = trim(std::move(token));
    const auto T = upper(token);
    if (T.rfind("UPPER(", 0) == 0 && !token.empty() && token.back()==')') {
        fn = Fn::Upper; inner = token.substr(6, token.size()-7); return true;
    }
    if (T.rfind("LOWER(", 0) == 0 && !token.empty() && token.back()==')') {
        fn = Fn::Lower; inner = token.substr(6, token.size()-7); return true;
    }
    fn = Fn::None; inner.clear(); return false;
}

// triplet evaluator: <field-or-func> <op> <rhs>
bool predx::eval_triplet(xbase::DbArea& A, std::string field, std::string op, std::string rhs) {
    Fn lfn = Fn::None; std::string linner;
    const bool hasFunc = parse_functor_token(field, lfn, linner);

    int fi = -1;
    std::string lhs;

    if (hasFunc) {
        fi = resolve_field_index(A, linner);
        lhs = (fi >= 0) ? get_cur_string(A, fi) : dequote(trim(linner));
        if (lfn == Fn::Upper) lhs = upper(std::move(lhs));
        else if (lfn == Fn::Lower) lhs = lower(std::move(lhs));
    } else {
        fi = resolve_field_index(A, field);
        if (fi >= 0) {
            lhs = get_cur_string(A, fi);
        } else {
            bool v=false;
            if (try_const_compare(dequote(trim(field)), trim(op), dequote(trim(rhs)), v)) return v;
            return false;
        }
    }

    std::string rhsv = dequote(trim(std::move(rhs)));
    const std::string uop = upper(trim(op));

    // numeric/date range ops
    if (uop==">" || uop==">=" || uop=="<" || uop=="<=") {
        double ln=0.0, rn=0.0;
        if (!parse_double(lhs, ln)) return false;
        if (!parse_double(rhsv, rn)) return false;
        return cmp_number(ln, uop, rn);
    }

    // boolean first, then numeric, else string
    if (is_bool_true(lhs) || is_bool_false(lhs)) {
        bool lb = is_bool_true(lhs);
        if (!(is_bool_true(rhsv) || is_bool_false(rhsv))) return false;
        bool rb = is_bool_true(rhsv);
        return cmp_bool(lb, uop, rb);
    }

    double ln=0.0, rn=0.0;
    if (parse_double(lhs, ln) && parse_double(rhsv, rn)) {
        return cmp_number(ln, uop, rn);
    }

    return cmp_string(lhs, uop, rhsv);
}

// expression evaluator: "FOR ...", or bare, or constants like ".T." / "1 = 1"
bool predx::eval_expr(xbase::DbArea& A, std::string expr) {
    expr = trim(std::move(expr));
    if (expr.size() >= 3 && ieq(expr.substr(0,3), "FOR")) {
        expr.erase(0,3);
        while (!expr.empty() && (expr.front()==' ' || expr.front()=='\t'))
            expr.erase(expr.begin());
        expr = trim(std::move(expr));
    }

    bool cb=false;
    if (try_const_bool(expr, cb)) return cb;

    std::istringstream is(expr);
    std::string lhsTok, op, rest;
    if (!(is >> lhsTok)) return false;
    if (!(is >> op))     return false;
    std::getline(is, rest);
    return predx::eval_triplet(A, lhsTok, op, rest);
}



