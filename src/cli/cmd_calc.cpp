// ===============================
// FILE: src/cli/cmd_calc.cpp
// First refactor cut: CALC routes RHS evaluation through expr::eval_rhs.
// ===============================
#include "cli/cmd_calc.hpp"

#include <cctype>
#include <cmath>
#include <iomanip>
#include <iostream>
#include <sstream>
#include <string>

#include "cli/expr/rhs_eval.hpp"
#include "cli/expr/value_eval.hpp"
#include "cli_comment.hpp"
#include "xbase.hpp"

// Forward decl (defined in cmd_calcwrite.cpp in your tree)
void cmd_CALCWRITE(xbase::DbArea& area, std::istringstream& args);

namespace {

static inline std::string trim(std::string s) {
    size_t b = 0;
    while (b < s.size() && std::isspace(static_cast<unsigned char>(s[b]))) ++b;
    size_t e = s.size();
    while (e > b && std::isspace(static_cast<unsigned char>(s[e - 1]))) --e;
    return s.substr(b, e - b);
}

static std::string strip_outer_parens(std::string s) {
    s = trim(std::move(s));
    for (;;) {
        if (s.size() < 2) return s;
        if (s.front() != '(' || s.back() != ')') return s;

        int depth = 0;
        bool in_s = false, in_d = false;
        bool ok_outer = true;

        for (size_t i = 0; i < s.size(); ++i) {
            const char c = s[i];
            if (c == '"' && !in_s) { in_d = !in_d; continue; }
            if (c == '\'' && !in_d) { in_s = !in_s; continue; }
            if (in_s || in_d) continue;
            if (c == '(') ++depth;
            else if (c == ')') --depth;
            if (depth == 0 && i != s.size() - 1) {
                ok_outer = false;
                break;
            }
        }

        if (!ok_outer || depth != 0) return s;
        s = trim(s.substr(1, s.size() - 2));
    }
}

static std::string up(std::string s) {
    for (char& c : s) c = static_cast<char>(std::toupper(static_cast<unsigned char>(c)));
    return s;
}

static bool parse_assign(const std::string& src, std::string& lhs, std::string& rhs) {
    bool in_s = false, in_d = false;
    int depth = 0;

    for (size_t i = 0; i < src.size(); ++i) {
        const char c = src[i];

        if (c == '"' && !in_s) { in_d = !in_d; continue; }
        if (c == '\'' && !in_d) { in_s = !in_s; continue; }
        if (in_s || in_d) continue;

        if (c == '(') { ++depth; continue; }
        if (c == ')') { if (depth > 0) --depth; continue; }

        if (depth == 0 && c == '=') {
            const char prev = (i > 0) ? src[i - 1] : ' ';
            const char next = (i + 1 < src.size()) ? src[i + 1] : ' ';
            if (prev == '<' || prev == '>' || prev == '!' || prev == '=' || next == '=') continue;

            lhs = trim(src.substr(0, i));
            rhs = trim(src.substr(i + 1));
            return !lhs.empty() && !rhs.empty();
        }
    }

    return false;
}

static int field_index_ci(xbase::DbArea& area, const std::string& name) {
    const std::string want = up(trim(name));
    if (want.empty()) return -1;

    const auto defs = area.fields();
    for (size_t i = 0; i < defs.size(); ++i) {
        std::string got = defs[i].name;
        for (char& c : got) c = static_cast<char>(std::toupper(static_cast<unsigned char>(c)));
        if (got == want) return static_cast<int>(i) + 1;
    }
    return -1;
}

static std::string format_number(double v) {
    const double iv = std::floor(v);
    if (std::fabs(v - iv) < 1e-9) {
        std::ostringstream o;
        o << static_cast<long long>(iv);
        return o.str();
    }

    std::ostringstream o;
    o << std::fixed << std::setprecision(10) << v;
    std::string s = o.str();

    while (!s.empty() && s.find('.') != std::string::npos && s.back() == '0') s.pop_back();
    if (!s.empty() && s.back() == '.') s.pop_back();

    return s.empty() ? "0" : s;
}

} // namespace

void cmd_CALC(xbase::DbArea& area, std::istringstream& args) {
    std::string expr;
    std::getline(args, expr);
    expr = cliutil::strip_inline_comments(trim(expr));

    if (expr.empty()) {
        std::cout << ".F.\n";
        return;
    }

    expr = strip_outer_parens(expr);

    std::string lhs, rhs;
    if (parse_assign(expr, lhs, rhs) && area.isOpen() && field_index_ci(area, lhs) > 0) {
        std::istringstream pass(lhs + " = " + rhs);
        cmd_CALCWRITE(area, pass);
        return;
    }

    std::string err;
    const dottalk::expr::EvalValue ev =
        dottalk::expr::eval_rhs(area.isOpen() ? &area : nullptr, expr, &err);

    switch (ev.kind) {
        case dottalk::expr::EvalValue::K_Bool:
            std::cout << (ev.tf ? ".T." : ".F.") << "\n";
            return;

        case dottalk::expr::EvalValue::K_Number:
            std::cout << format_number(ev.number) << "\n";
            return;

        case dottalk::expr::EvalValue::K_String:
            std::cout << ev.text << "\n";
            return;

        case dottalk::expr::EvalValue::K_Date:
            std::cout << ev.date8 << "\n";
            return;

        default:
            if (!area.isOpen()) std::cout << "CALC: no file open.\n";
            else std::cout << ".F.\n";
            return;
    }
}