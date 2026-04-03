#include "shell_eval_utils.hpp"

#include "textio.hpp"
#include "expr/date/date_utils.hpp"
#include "cli/expr/value_eval.hpp"
#include "shell_lexicon.hpp"

#include <algorithm>
#include <cctype>
#include <sstream>
#include <cmath>
#include <iomanip>  // for std::setprecision

namespace dottalk {

static inline bool is_arith_only(const std::string& s) {
    bool has_digit = false;
    for (char c : s) {
        if (std::isspace(static_cast<unsigned char>(c))) continue;
        if (std::isdigit(static_cast<unsigned char>(c))) { has_digit = true; continue; }
        if (c == '+' || c == '-' || c == '*' || c == '/' || c == '(' || c == ')' || c == '.') continue;
        return false;
    }
    return has_digit;
}

struct ArithReader {
    const char* p; const char* e;
    void skip() { while (p < e && std::isspace(static_cast<unsigned char>(*p))) ++p; }
    bool eat(char c) { skip(); if (p < e && *p == c) { ++p; return true; } return false; }
    bool end() { skip(); return p >= e; }
    double number(bool& ok) {
        skip();
        const char* s = p;
        if (p < e && (*p == '+' || *p == '-')) ++p;
        bool any = false;
        while (p < e && std::isdigit(static_cast<unsigned char>(*p))) { ++p; any = true; }
        if (p < e && *p == '.') { ++p; while (p < e && std::isdigit(static_cast<unsigned char>(*p))) { ++p; any = true; } }
        if (!any) { ok = false; return 0.0; }
        return std::strtod(s, nullptr);
    }
    double parse_factor(bool& ok) {
        skip();
        if (eat('(')) {
            double v = parse_expr(ok);
            if (!eat(')')) ok = false;
            return v;
        }
        if (p < e && (*p == '+' || *p == '-')) {
            bool neg = (*p == '-'); ++p;
            double v = parse_factor(ok);
            return neg ? -v : v;
        }
        return number(ok);
    }
    double parse_term(bool& ok) {
        double v = parse_factor(ok);
        while (true) {
            if (eat('*')) { double r = parse_factor(ok); v *= r; }
            else if (eat('/')) { double r = parse_factor(ok); v /= r; }
            else break;
        }
        return v;
    }
    double parse_expr(bool& ok) {
        double v = parse_term(ok);
        while (true) {
            if (eat('+')) { double r = parse_term(ok); v += r; }
            else if (eat('-')) { double r = parse_term(ok); v -= r; }
            else break;
        }
        return v;
    }
};

static bool try_eval_simple_arith(const std::string& src, double& out) {
    if (!is_arith_only(src)) return false;
    ArithReader rd{src.data(), src.data() + src.size()};
    bool ok = true;
    double v = rd.parse_expr(ok);
    if (!ok || !rd.end()) return false;
    out = v;
    return true;
}

static inline bool is_quoted_literal(const std::string& s) {
    if (s.size() < 2) return false;
    const char a = s.front(), b = s.back();
    return ((a == '"' && b == '"') || (a == '\'' && b == '\''));
}

static std::string quote_dottalk_string(const std::string& raw) {
    std::string out;
    out.reserve(raw.size() + 2);
    out.push_back('"');
    for (char c : raw) {
        if (c == '"') out.append("\"\"");
        else out.push_back(c);
    }
    out.push_back('"');
    return out;
}

bool eval_for_varbang(xbase::DbArea& A, const std::string& expr, VarBangEval& out, std::string& err) {
    std::string src = textio::trim(expr);
    if (src.empty()) { err = "empty expression"; return false; }

    // Clock-only fast path
    const auto cs = dottalk::date::now_local();
    const std::string U = textio::up(src);
    if (U == "DATE()" || U == "TODAY()") {
        out.kind = VarBangEval::K_Date8;
        out.text = cs.date8;
        return true;
    }
    if (U == "TIME()") {
        out.kind = VarBangEval::K_String;
        out.text = cs.time6;
        return true;
    }
    if (U == "NOW()" || U == "DATETIME()") {
        out.kind = VarBangEval::K_String;
        out.text = cs.datetime14;
        return true;
    }

    // CTOD fast path
    {
        auto d8 = dottalk::date::parse_ctod(src);
        if (!d8.empty()) {
            out.kind = VarBangEval::K_Date8;
            out.text = d8;
            return true;
        }
    }

    // No DBF: simple arith / literal
    if (!A.isOpen()) {
        double v = 0.0;
        if (try_eval_simple_arith(src, v)) {
            out.kind = VarBangEval::K_Number;
            out.number = v;
            return true;
        }
        if (is_quoted_literal(src)) {
            out.kind = VarBangEval::K_String;
            out.text = src.substr(1, src.size() - 2);
            return true;
        }
        err = "no file open (expression requires fields/functions)";
        return false;
    }

    // Full evaluation pipeline (with builtin expansion for DATEADD/DTOC/etc.)
    dottalk::expr::EvalValue ev = dottalk::expr::eval_any(A, src, &err);
    if (ev.kind == dottalk::expr::EvalValue::K_None) {
        err = err.empty() ? "unable to evaluate expression" : err;
        return false;
    }

    switch (ev.kind) {
        case dottalk::expr::EvalValue::K_Number:
            out.kind = VarBangEval::K_Number;
            out.number = ev.number;
            return true;
        case dottalk::expr::EvalValue::K_String:
            out.kind = VarBangEval::K_String;
            out.text = ev.text;
            return true;
        case dottalk::expr::EvalValue::K_Bool:
            out.kind = VarBangEval::K_Bool;
            out.tf = ev.tf;
            return true;
        default:
            err = "unexpected eval kind";
            return false;
    }
}

std::string serialize_varbang_value(const VarBangEval& v) {
    if (v.kind == VarBangEval::K_Bool) return v.tf ? ".T." : ".F.";
    if (v.kind == VarBangEval::K_Number) {
        std::ostringstream oss;
        oss.imbue(std::locale::classic());
        double x = v.number;
        long long xi = (long long)std::llround(x);
        if (std::fabs(x - (double)xi) < 1e-9) {
            oss << xi;
        } else {
            oss << std::setprecision(15) << x;
        }
        return textio::trim(oss.str());
    }
    if (v.kind == VarBangEval::K_Date8 || v.kind == VarBangEval::K_String) {
        return quote_dottalk_string(v.text);
    }
    return std::string{};
}

} // namespace dottalk