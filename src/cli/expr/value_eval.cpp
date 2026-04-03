// src/cli/expr/value_eval.cpp
#include "cli/expr/value_eval.hpp"

#include <algorithm>
#include <cctype>
#include <cstdlib>
#include <memory>
#include <optional>
#include <sstream>
#include <string>
#include <utility>
#include <vector>

#include "xbase.hpp"

// IMPORTANT: compile_where lives here
#include "cli/expr/api.hpp"

#include "cli/expr/ast.hpp"
#include "cli/expr/fn_date.hpp"
#include "cli/expr/fn_string.hpp"
#include "cli/expr/glue_xbase.hpp"

// value_eval.cpp is in src/cli/expr/, predicate_chain.hpp is in src/cli/
#include "../predicate_chain.hpp"

namespace {

// -------------------- tiny text helpers --------------------

static inline std::string ltrim(std::string s) {
    size_t i = 0;
    while (i < s.size() && std::isspace(static_cast<unsigned char>(s[i]))) ++i;
    return s.substr(i);
}

static inline std::string rtrim(std::string s) {
    while (!s.empty() && std::isspace(static_cast<unsigned char>(s.back()))) s.pop_back();
    return s;
}

static inline std::string trim(std::string s) { return rtrim(ltrim(std::move(s))); }

static inline std::string up(std::string s) {
    for (char& ch : s) ch = static_cast<char>(std::toupper(static_cast<unsigned char>(ch)));
    return s;
}

static int resolve_field_index_by_name_ci(const xbase::DbArea& A, const std::string& nameIn) {
    const std::string want = up(trim(nameIn));
    const auto F = A.fields();
    for (int i = 0; i < static_cast<int>(F.size()); ++i) {
        if (up(trim(F[static_cast<size_t>(i)].name)) == want) return i + 1; // 1-based
    }
    return -1;
}

static std::optional<bool> try_eval_predicate_chain_fast(const xbase::DbArea& A, const std::string& src) {
    bool out = false;
    std::string err;
    if (!::try_eval_predicate_chain(A, src, out, &err)) return std::nullopt;
    return out;
}

static bool predicate_chain_fast_allowed(const std::string& src) {
    // Only allow the predicate-chain shortcut for simple WHERE-style predicates.
    // Complex expressions (parens, arithmetic, quotes, function calls) must go through compile_where.
    for (char c : src) {
        if (c == '"' || c == '\'' || c == '(' || c == ')' || c == ',' || c == '+' || c == '*' || c == '/')
            return false;
    }
    // '-' usually indicates arithmetic (or unary minus); keep it on the slow path.
    if (src.find('-') != std::string::npos) return false;
    return true;
}

// -------------------- “value-expr to string” subset --------------------
// Field refs + literals + string/date builtins. No arithmetic operators.

struct Tok {
    enum Kind { Ident, Number, String, LParen, RParen, Comma, End } kind{};
    std::string text;
};

static bool is_ident_start(char c) {
    return std::isalpha(static_cast<unsigned char>(c)) || c == '_';
}
static bool is_ident_char(char c) {
    return std::isalnum(static_cast<unsigned char>(c)) || c == '_';
}

static std::vector<Tok> lex_value_expr(const std::string& src) {
    std::vector<Tok> out;
    out.reserve(src.size() / 2 + 8);

    auto skip_ws = [&](size_t& i) {
        while (i < src.size() && std::isspace(static_cast<unsigned char>(src[i]))) ++i;
    };

    for (size_t i = 0; i < src.size();) {
        skip_ws(i);
        if (i >= src.size()) break;

        const char c = src[i];

        if (c == '(') { out.push_back({Tok::LParen, "("}); ++i; continue; }
        if (c == ')') { out.push_back({Tok::RParen, ")"}); ++i; continue; }
        if (c == ',') { out.push_back({Tok::Comma, ","}); ++i; continue; }

        if (c == '\'' || c == '"') {
            const char q = c;
            ++i;
            std::string s;
            while (i < src.size()) {
                char d = src[i++];
                if (d == q) {
                    if (i < src.size() && src[i] == q) { s.push_back(q); ++i; continue; }
                    break;
                }
                s.push_back(d);
            }
            out.push_back({Tok::String, std::move(s)});
            continue;
        }

        if (std::isdigit(static_cast<unsigned char>(c)) ||
            (c == '.' && i + 1 < src.size() && std::isdigit(static_cast<unsigned char>(src[i + 1])))) {
            size_t j = i;
            bool dot = false;
            while (j < src.size()) {
                const char d = src[j];
                if (std::isdigit(static_cast<unsigned char>(d))) { ++j; continue; }
                if (d == '.' && !dot) { dot = true; ++j; continue; }
                break;
            }
            out.push_back({Tok::Number, src.substr(i, j - i)});
            i = j;
            continue;
        }

        if (is_ident_start(c)) {
            size_t j = i + 1;
            while (j < src.size() && is_ident_char(src[j])) ++j;
            out.push_back({Tok::Ident, src.substr(i, j - i)});
            i = j;
            continue;
        }

        break; // unknown char => stop
    }

    out.push_back({Tok::End, ""});
    return out;
}

class ValueParser {
public:
    ValueParser(xbase::DbArea& A, const std::vector<Tok>& t) : area_(A), toks_(t) {}

    bool parse_expr(std::string& out) { return parse_primary(out); }
    bool at_end() const { return peek().kind == Tok::End; }

private:
    xbase::DbArea& area_;
    const std::vector<Tok>& toks_;
    size_t pos_{0};

    const Tok& peek() const { return toks_[pos_]; }

    bool accept(Tok::Kind k) {
        if (peek().kind == k) { ++pos_; return true; }
        return false;
    }

    bool expect(Tok::Kind k) { return accept(k); }

    bool parse_primary(std::string& out) {
        const Tok& t = peek();

        if (t.kind == Tok::String || t.kind == Tok::Number) {
            out = t.text;
            ++pos_;
            return true;
        }

        if (t.kind == Tok::Ident) {
            std::string ident = t.text;
            ++pos_;

            if (accept(Tok::LParen)) {
                std::vector<std::string> args;
                if (!accept(Tok::RParen)) {
                    while (true) {
                        std::string a;
                        if (!parse_expr(a)) return false;
                        args.push_back(a);
                        if (accept(Tok::Comma)) continue;
                        if (!expect(Tok::RParen)) return false;
                        break;
                    }
                }

                const std::string fn = up(ident);

                // string fns
                {
                    const auto* specs = dottalk::expr::string_fn_specs();
                    const std::size_t n = dottalk::expr::string_fn_specs_count();

                    for (std::size_t i = 0; i < n; ++i) {
                        const auto& s = specs[i];
                        if (fn == s.name) {
                            const int argc = static_cast<int>(args.size());
                            if (argc < s.minArgs || argc > s.maxArgs) return false;
                            out = s.fn(args);
                            return true;
                        }
                    }
                }

                // date fns
                {
                    const auto* specs = dottalk::expr::date_fn_specs();
                    const std::size_t n = dottalk::expr::date_fn_specs_count();

                    for (std::size_t i = 0; i < n; ++i) {
                        const auto& s = specs[i];
                        if (fn == s.name) {
                            const int argc = static_cast<int>(args.size());
                            if (argc < s.minArgs || argc > s.maxArgs) return false;
                            out = s.fn(args);
                            return true;
                        }
                    }
                }

                return false;
            }

            // field reference?
            int fld = resolve_field_index_by_name_ci(area_, ident);
            if (fld > 0) {
                try { out = area_.get(fld); }
                catch (...) { out.clear(); }
                return true;
            }

            // bare identifier => literal text
            out = ident;
            return true;
        }

        return false;
    }
};

// ------------------------------------------------------------
// Builtin expansion + constant date algebra folding helpers
// ------------------------------------------------------------

struct FnCallSpan {
    size_t start = 0;
    size_t end   = 0; // inclusive
    int    depth = 0;
    std::string nameUpper;
};

static bool is_known_value_fn_upper(const std::string& nameUpper) {
    const auto* s = dottalk::expr::string_fn_specs();
    for (int i = 0; i < dottalk::expr::string_fn_specs_count(); ++i) {
        if (nameUpper == s[i].name) return true;
    }
    const auto* d = dottalk::expr::date_fn_specs();
    for (int i = 0; i < dottalk::expr::date_fn_specs_count(); ++i) {
        if (nameUpper == d[i].name) return true;
    }
    return false;
}

static bool is_numeric_literal(const std::string& s) {
    if (s.empty()) return false;
    size_t i = 0;
    if (s[0] == '+' || s[0] == '-') i = 1;
    bool any = false;
    bool dot = false;
    for (; i < s.size(); ++i) {
        const unsigned char c = static_cast<unsigned char>(s[i]);
        if (std::isdigit(c)) { any = true; continue; }
        if (c == '.' && !dot) { dot = true; continue; }
        return false;
    }
    return any;
}

static std::string quote_for_expr(const std::string& raw) {
    std::string out;
    out.reserve(raw.size() + 2);
    out.push_back('"');
    for (char ch : raw) {
        if (ch == '\\' || ch == '"') out.push_back('\\');
        out.push_back(ch);
    }
    out.push_back('"');
    return out;
}

static std::vector<FnCallSpan> find_value_fn_calls(const std::string& s) {
    std::vector<FnCallSpan> calls;

    bool in_dq = false;
    bool in_sq = false;
    int depth = 0;

    for (size_t i = 0; i < s.size();) {
        const char c = s[i];

        // quote state
        if (in_dq) {
            if (c == '\\' && i + 1 < s.size()) { i += 2; continue; }
            if (c == '"') in_dq = false;
            ++i;
            continue;
        }
        if (in_sq) {
            if (c == '\\' && i + 1 < s.size()) { i += 2; continue; }
            if (c == '\'') in_sq = false;
            ++i;
            continue;
        }
        if (c == '"') { in_dq = true; ++i; continue; }
        if (c == '\'') { in_sq = true; ++i; continue; }

        // depth tracking for surrounding parens
        if (c == '(') { ++depth; ++i; continue; }
        if (c == ')') { if (depth > 0) --depth; ++i; continue; }

        // identifier -> possible call
        if (std::isalpha(static_cast<unsigned char>(c)) || c == '_') {
            const size_t start = i;
            size_t j = i + 1;
            while (j < s.size()) {
                const char cj = s[j];
                if (std::isalnum(static_cast<unsigned char>(cj)) || cj == '_') ++j;
                else break;
            }
            const std::string ident = s.substr(start, j - start);

            // skip ws before '('
            size_t k = j;
            while (k < s.size() && std::isspace(static_cast<unsigned char>(s[k]))) ++k;

            if (k < s.size() && s[k] == '(') {
                const size_t open = k;

                // find matching ')', respecting quotes and nested parens
                int p = 1;
                bool dq = false;
                bool sq = false;
                size_t t = open + 1;
                for (; t < s.size(); ++t) {
                    const char ct = s[t];

                    if (dq) {
                        if (ct == '\\' && t + 1 < s.size()) { ++t; continue; }
                        if (ct == '"') dq = false;
                        continue;
                    }
                    if (sq) {
                        if (ct == '\\' && t + 1 < s.size()) { ++t; continue; }
                        if (ct == '\'') sq = false;
                        continue;
                    }

                    if (ct == '"') { dq = true; continue; }
                    if (ct == '\'') { sq = true; continue; }

                    if (ct == '(') ++p;
                    else if (ct == ')') {
                        --p;
                        if (p == 0) break;
                    }
                }

                if (p == 0) {
                    calls.push_back(FnCallSpan{ start, t, depth, up(ident) });
                    i = t + 1;
                    continue;
                }
            }

            i = j;
            continue;
        }

        ++i;
    }

    return calls;
}

static bool call_date_builtin_upper(const std::string& nameUpper,
                                   const std::vector<std::string>& argv,
                                   std::string& out) {
    const auto* specs = dottalk::expr::date_fn_specs();
    const std::size_t n = dottalk::expr::date_fn_specs_count();
    for (std::size_t i = 0; i < n; ++i) {
        const auto& s = specs[i];
        if (nameUpper == s.name) {
            const int argc = static_cast<int>(argv.size());
            if (argc < s.minArgs || argc > s.maxArgs) return false;
            try {
                out = s.fn(argv);
                return true;
            } catch (...) {
                return false;
            }
        }
    }
    return false;
}

static std::string expand_value_builtins_in_text(xbase::DbArea& A, const std::string& exprText) {
    std::string s = exprText;

    // Iteratively expand deepest calls first (handles nesting).
    for (int iter = 0; iter < 16; ++iter) {
        const auto calls = find_value_fn_calls(s);

        std::vector<FnCallSpan> known;
        known.reserve(calls.size());
        int maxDepth = -1;

        for (const auto& c : calls) {
            if (!is_known_value_fn_upper(c.nameUpper)) continue;
            known.push_back(c);
            if (c.depth > maxDepth) maxDepth = c.depth;
        }

        if (known.empty()) break;

        std::vector<FnCallSpan> deepest;
        deepest.reserve(known.size());
        for (const auto& c : known) {
            if (c.depth == maxDepth) deepest.push_back(c);
        }
        if (deepest.empty()) break;

        // Replace from end so indices stay valid for disjoint spans.
        std::sort(deepest.begin(), deepest.end(),
                  [](const FnCallSpan& a, const FnCallSpan& b) { return a.start > b.start; });

        bool changed = false;

        for (const auto& c : deepest) {
            if (c.start >= s.size() || c.end >= s.size() || c.end < c.start) continue;

            const size_t len = c.end - c.start + 1;
            const std::string callText = s.substr(c.start, len);

            std::string out;
            if (!dottalk::expr::eval_string_value_expr(A, callText, out)) continue;

            const std::string literal = is_numeric_literal(out) ? out : quote_for_expr(out);
            s.replace(c.start, len, literal);
            changed = true;
        }

        if (!changed) break;
    }

    return s;
}

// Fold constant date algebra like:
//   20000101 + 30
//   20000101 - 1
// into DATEADD results so comparisons behave as date arithmetic, not numeric arithmetic.
static std::string fold_constant_date_algebra_in_text(xbase::DbArea& A, const std::string& exprText) {
    std::string s = exprText;

    auto is_digit = [](unsigned char ch) { return std::isdigit(ch) != 0; };
    auto is_space = [](unsigned char ch) { return std::isspace(ch) != 0; };

    auto skip_ws = [&](size_t pos) {
        while (pos < s.size() && is_space(static_cast<unsigned char>(s[pos]))) ++pos;
        return pos;
    };

    for (int iter = 0; iter < 32; ++iter) {
        bool changed = false;
        bool in_sq = false;
        bool in_dq = false;
        bool esc = false;

        for (size_t i = 0; i < s.size(); ++i) {
            const char c = s[i];

            // Track quotes so we don't rewrite inside string literals.
            if (esc) { esc = false; continue; }
            if (c == '\\') { esc = true; continue; }
            if (in_dq) { if (c == '"') in_dq = false; continue; }
            if (in_sq) { if (c == '\'') in_sq = false; continue; }
            if (c == '"') { in_dq = true; continue; }
            if (c == '\'') { in_sq = true; continue; }

            if (!is_digit(static_cast<unsigned char>(c))) continue;

            // LHS: exactly 8 digits (date8)
            size_t j = i;
            while (j < s.size() && is_digit(static_cast<unsigned char>(s[j]))) ++j;
            const size_t lhs_len = j - i;
            if (lhs_len != 8) { i = j; continue; }

            // boundary check
            if (i > 0 && is_digit(static_cast<unsigned char>(s[i - 1]))) { i = j; continue; }
            if (j < s.size() && is_digit(static_cast<unsigned char>(s[j]))) { i = j; continue; }

            size_t k = skip_ws(j);
            if (k >= s.size() || (s[k] != '+' && s[k] != '-')) { i = j; continue; }
            const char op = s[k];

            size_t r0 = skip_ws(k + 1);
            if (r0 >= s.size() || !is_digit(static_cast<unsigned char>(s[r0]))) { i = j; continue; }

            size_t r1 = r0;
            while (r1 < s.size() && is_digit(static_cast<unsigned char>(s[r1]))) ++r1;
            const size_t rhs_len = r1 - r0;

            const std::string lhs_date8 = s.substr(i, 8);
            const std::string rhs_token = s.substr(r0, rhs_len);

            // date + int  OR  date - int
            if (rhs_len >= 1 && rhs_len <= 10) {
                long long days = 0;
                try { days = std::stoll(rhs_token); }
                catch (...) { i = r1; continue; }

                if (op == '-') days = -days;

                std::string out;
                if (!call_date_builtin_upper("DATEADD", { lhs_date8, std::to_string(days) }, out)) {
                    i = r1; continue;
                }

                // Replace the whole "YYYYMMDD <op> N" span with computed date8 (numeric literal)
                s.replace(i, r1 - i, out);
                changed = true;
                break;
            }

            // date - date => datediff
            if (op == '-' && rhs_len == 8) {
                std::string out;
                if (!call_date_builtin_upper("DATEDIFF", { lhs_date8, rhs_token }, out)) {
                    i = r1; continue;
                }
                s.replace(i, r1 - i, out);
                changed = true;
                break;
            }

            i = r1;
        }

        if (!changed) break;
    }

    (void)A; // reserved: A may be used for future field-aware rewrites
    return s;
}

} // namespace

namespace dottalk::expr {

// NOTE: these two must have external linkage (NOT static, NOT in anonymous ns)
// because other translation units link against them.

bool compile_where_program(const std::string& exprText,
                           std::unique_ptr<Expr>& programOut,
                           std::string* errOut) {
    programOut.reset();

    auto cr = dottalk::expr::compile_where(exprText);
    if (!cr) {
        if (errOut) *errOut = cr.error.empty() ? "compile_where failed" : cr.error;
        return false;
    }

    programOut = std::move(cr.program);
    if (!programOut) {
        if (errOut) *errOut = "compile_where returned no program";
        return false;
    }
    return true;
}

bool eval_string_value_expr(xbase::DbArea& A, const std::string& exprText, std::string& out) {
    const auto toks = lex_value_expr(exprText);
    ValueParser p(A, toks);
    std::string v;
    if (!p.parse_expr(v)) return false;
    if (!p.at_end()) return false;
    out = v;
    return true;
}

EvalValue eval_compiled_program(const Expr* prog, const RecordView& rv) {
    EvalValue ev{};
    if (!prog) return ev;

    if (auto ln = dynamic_cast<const LitNumber*>(prog)) {
        ev.kind = EvalValue::K_Number;
        ev.number = ln->v;
        return ev;
    }
    if (auto ls = dynamic_cast<const LitString*>(prog)) {
        ev.kind = EvalValue::K_String;
        ev.text = ls->v;
        return ev;
    }
    if (auto ar = dynamic_cast<const Arith*>(prog)) {
        try {
            const double n = ar->evalNumber(rv);

            // Bridge rule for folded date arithmetic:
            // expressions like CTOD("2000-01-01") - 1 are folded into
            // numeric date8 literals before AST compilation. Preserve them
            // as date values instead of flattening them to plain numbers.
            if (n >= 10000101.0 && n <= 99991231.0) {
                const long long iv = static_cast<long long>(n);
                ev.kind = EvalValue::K_Date;
                ev.date8 = static_cast<std::int32_t>(iv);
                return ev;
            }

            ev.kind = EvalValue::K_Number;
            ev.number = n;
            return ev;
        } catch (...) {
            // fall through
        }
    }

    try {
        ev.kind = EvalValue::K_Bool;
        ev.tf = prog->eval(rv);
        return ev;
    } catch (...) {
        ev.kind = EvalValue::K_None;
        return ev;
    }

    return EvalValue{};
}

EvalValue eval_any(xbase::DbArea& A, const std::string& exprText, std::string* errOut) {
    const std::string expandedCalls = expand_value_builtins_in_text(A, exprText);
    const std::string foldedDates   = fold_constant_date_algebra_in_text(A, expandedCalls);
    const std::string src = trim(foldedDates);
    if (src.empty()) return EvalValue{};

    // 1) boolean chain fast path (simple predicates only)
    if (predicate_chain_fast_allowed(src)) {
        if (auto b = try_eval_predicate_chain_fast(A, src); b.has_value()) {
            EvalValue ev{};
            ev.kind = EvalValue::K_Bool;
            ev.tf = b.value();
            return ev;
        }
    }

    // 2) compile_where -> program eval
    {
        std::unique_ptr<Expr> prog;
        std::string err;
        if (compile_where_program(src, prog, &err)) {
            auto rv = dottalk::expr::glue::make_record_view(A);
            return eval_compiled_program(prog.get(), rv);
        }
    }

    // 3) fallback: string value-expr subset
    {
        std::string s;
        if (eval_string_value_expr(A, src, s)) {
            EvalValue ev{};
            ev.kind = EvalValue::K_String;
            ev.text = s;
            return ev;
        }
    }

    if (errOut) *errOut = "unable to evaluate expression";
    return EvalValue{};
}

bool eval_bool(xbase::DbArea& A, const std::string& exprText, bool& out, std::string* errOut) {
    const std::string expandedCalls = expand_value_builtins_in_text(A, exprText);
    const std::string foldedDates   = fold_constant_date_algebra_in_text(A, expandedCalls);
    const std::string src = trim(foldedDates);
    if (src.empty()) { out = true; return true; }

    // 1) boolean chain fast path (simple predicates only)
    if (predicate_chain_fast_allowed(src)) {
        if (auto b = try_eval_predicate_chain_fast(A, src); b.has_value()) {
            out = b.value();
            return true;
        }
    }

    // 2) compile_where -> program eval
    std::unique_ptr<Expr> prog;
    std::string err;
    if (!compile_where_program(src, prog, &err)) {
        if (errOut) *errOut = err;
        return false;
    }

    auto rv = dottalk::expr::glue::make_record_view(A);
    const EvalValue ev = eval_compiled_program(prog.get(), rv);

    if (ev.kind == EvalValue::K_Bool) { out = ev.tf; return true; }
    if (ev.kind == EvalValue::K_Number) { out = (ev.number != 0.0); return true; }

    if (errOut) *errOut = "FOR/WHILE must evaluate to logical/boolean (or numeric truthy)";
    return false;
}

} // namespace dottalk::expr