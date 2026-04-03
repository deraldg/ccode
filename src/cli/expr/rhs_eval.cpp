
#include "cli/expr/rhs_eval.hpp"

#include <algorithm>
#include <cctype>
#include <cmath>
#include <cstdint>
#include <cstdlib>
#include <iomanip>
#include <optional>
#include <sstream>
#include <string>
#include <string_view>
#include <type_traits>
#include <utility>
#include <vector>

#include "cli/expr/evaluate.hpp"
#include "cli/expr/fn_date.hpp"
#include "cli/expr/fn_string.hpp"
#include "cli/expr/value_eval.hpp"
#include "memo/memo_auto.hpp"
#include "memo/memostore.hpp"
#include "xbase.hpp"
#include "xbase_64.hpp"

namespace {

template <typename T>
static int spec_min_args(const T& s) {
    if constexpr (requires { s.minArgs; }) return s.minArgs;
    else return s.min_args;
}

template <typename T>
static int spec_max_args(const T& s) {
    if constexpr (requires { s.maxArgs; }) return s.maxArgs;
    else return s.max_args;
}

static constexpr char kSingleQuote = 0x27;

static inline std::string trim(std::string s) {
    size_t b = 0;
    while (b < s.size() && std::isspace(static_cast<unsigned char>(s[b]))) ++b;
    size_t e = s.size();
    while (e > b && std::isspace(static_cast<unsigned char>(s[e - 1]))) --e;
    return s.substr(b, e - b);
}

static inline std::string up(std::string s) {
    for (char& c : s) c = static_cast<char>(std::toupper(static_cast<unsigned char>(c)));
    return s;
}

static std::string strip_outer_quotes(std::string s) {
    s = trim(std::move(s));
    if (s.size() >= 2) {
        const char a = s.front();
        const char b = s.back();
        if ((a == '"' && b == '"') || (a == kSingleQuote && b == kSingleQuote)) {
            s = s.substr(1, s.size() - 2);
        }
    }
    return trim(std::move(s));
}

static std::string digits_only(std::string_view sv) {
    std::string out;
    out.reserve(sv.size());
    for (char c : sv) {
        if (std::isdigit(static_cast<unsigned char>(c))) out.push_back(c);
    }
    return out;
}

static bool contains_quote(const std::string& src) {
    for (char c : src) {
        if (c == '"' || c == kSingleQuote) return true;
    }
    return false;
}

static bool is_ident_like(const std::string& s) {
    if (s.empty()) return false;
    if (!(std::isalpha(static_cast<unsigned char>(s[0])) || s[0] == '_')) return false;
    for (char c : s) {
        const unsigned char uc = static_cast<unsigned char>(c);
        if (!(std::isalnum(uc) || c == '_')) return false;
    }
    return true;
}

static bool looks_like_function_call(const std::string& src) {
    const std::string t = trim(src);
    if (t.empty()) return false;
    size_t i = 0;
    if (!(std::isalpha(static_cast<unsigned char>(t[i])) || t[i] == '_')) return false;
    size_t j = i + 1;
    while (j < t.size()) {
        const unsigned char c = static_cast<unsigned char>(t[j]);
        if (std::isalnum(c) || t[j] == '_') ++j;
        else break;
    }
    const std::string name = trim(t.substr(0, j));
    if (!is_ident_like(name)) return false;
    size_t k = j;
    while (k < t.size() && std::isspace(static_cast<unsigned char>(t[k]))) ++k;
    if (k >= t.size() || t[k] != '(') return false;
    bool in_s = false, in_d = false;
    int depth = 0;
    for (size_t p = k; p < t.size(); ++p) {
        const char c = t[p];
        if (c == '"' && !in_s) { in_d = !in_d; continue; }
        if (c == kSingleQuote && !in_d) { in_s = !in_s; continue; }
        if (in_s || in_d) continue;
        if (c == '(') ++depth;
        else if (c == ')') {
            --depth;
            if (depth == 0) {
                for (size_t q = p + 1; q < t.size(); ++q) {
                    if (!std::isspace(static_cast<unsigned char>(t[q]))) return false;
                }
                return true;
            }
        }
        if (depth < 0) return false;
    }
    return false;
}

static int field_index_ci(xbase::DbArea& area, const std::string& name) {
    const std::string want = up(trim(name));
    if (want.empty()) return -1;
    const auto defs = area.fields();
    for (size_t i = 0; i < defs.size(); ++i) {
        if (up(defs[i].name) == want) return static_cast<int>(i) + 1;
    }
    return -1;
}

static bool is_x64_memo_field(const xbase::DbArea& A, int field1) {
    if (field1 < 1 || field1 > A.fieldCount()) return false;
    if (A.versionByte() != xbase::DBF_VERSION_64) return false;
    const auto& f = A.fields()[static_cast<std::size_t>(field1 - 1)];
    return (f.type == 'M' || f.type == 'm') &&
           f.length == xbase::X64_MEMO_FIELD_LEN;
}

static std::uint64_t parse_u64_or_zero(const std::string& s) {
    if (s.empty()) return 0;
    try {
        std::size_t used = 0;
        const unsigned long long v = std::stoull(s, &used, 10);
        if (used != s.size()) return 0;
        return static_cast<std::uint64_t>(v);
    } catch (...) {
        return 0;
    }
}

static dottalk::memo::MemoStore* memo_store_for_area(xbase::DbArea& A) noexcept {
    auto* backend = cli_memo::memo_backend_for(A);
    if (!backend) return nullptr;
    return dynamic_cast<dottalk::memo::MemoStore*>(backend);
}

static std::string get_logical_field_text(xbase::DbArea& area, int field1) {
    if (field1 <= 0) return {};
    try {
        if (!is_x64_memo_field(area, field1)) return area.get(field1);
        const std::uint64_t oid = parse_u64_or_zero(area.get(field1));
        if (!oid) return {};
        auto* store = memo_store_for_area(area);
        if (!store) return {};
        std::string txt;
        if (!store->get_text_id(oid, txt, nullptr)) return {};
        return txt;
    } catch (...) {
        return {};
    }
}

struct Tok {
    enum Kind {
        Ident, Number, String, LParen, RParen, Comma,
        Plus, Minus, Star, Slash,
        Eq, Ne, Lt, Le, Gt, Ge,
        And, Or, Not,
        End
    } kind{};
    std::string text;
};

static bool is_ident_start(char c) {
    return std::isalpha(static_cast<unsigned char>(c)) || c == '_' || c == '$';
}
static bool is_ident_char(char c) {
    return std::isalnum(static_cast<unsigned char>(c)) || c == '_' || c == '$';
}

static std::vector<Tok> lex_value_expr(const std::string& src) {
    std::vector<Tok> out;
    std::size_t i = 0;
    auto skip_ws = [&]() {
        while (i < src.size() && std::isspace(static_cast<unsigned char>(src[i]))) ++i;
    };
    skip_ws();
    while (i < src.size()) {
        char c = src[i];
        if (std::isspace(static_cast<unsigned char>(c))) { skip_ws(); continue; }
        if (c == '(') { out.push_back({Tok::LParen, "("}); ++i; continue; }
        if (c == ')') { out.push_back({Tok::RParen, ")"}); ++i; continue; }
        if (c == ',') { out.push_back({Tok::Comma, ","}); ++i; continue; }
        if (c == '+') { out.push_back({Tok::Plus, "+"}); ++i; continue; }
        if (c == '-') { out.push_back({Tok::Minus, "-"}); ++i; continue; }
        if (c == '*') { out.push_back({Tok::Star, "*"}); ++i; continue; }
        if (c == '/') { out.push_back({Tok::Slash, "/"}); ++i; continue; }
        if (c == '<' && i + 1 < src.size() && src[i + 1] == '=') { out.push_back({Tok::Le, "<="}); i += 2; continue; }
        if (c == '>' && i + 1 < src.size() && src[i + 1] == '=') { out.push_back({Tok::Ge, ">="}); i += 2; continue; }
        if (c == '<' && i + 1 < src.size() && src[i + 1] == '>') { out.push_back({Tok::Ne, "<>"}); i += 2; continue; }
        if (c == '!' && i + 1 < src.size() && src[i + 1] == '=') { out.push_back({Tok::Ne, "!="}); i += 2; continue; }
        if (c == '=') { out.push_back({Tok::Eq, "="}); ++i; continue; }
        if (c == '<') { out.push_back({Tok::Lt, "<"}); ++i; continue; }
        if (c == '>') { out.push_back({Tok::Gt, ">"}); ++i; continue; }
        if (c == kSingleQuote || c == '"') {
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
            out.push_back({Tok::String, s});
            continue;
        }
        if (std::isdigit(static_cast<unsigned char>(c)) ||
            (c == '.' && i + 1 < src.size() && std::isdigit(static_cast<unsigned char>(src[i + 1])))) {
            std::size_t j = i;
            bool dot = false;
            while (j < src.size()) {
                char d = src[j];
                if (std::isdigit(static_cast<unsigned char>(d))) { ++j; continue; }
                if (d == '.' && !dot) { dot = true; ++j; continue; }
                break;
            }
            out.push_back({Tok::Number, src.substr(i, j - i)});
            i = j;
            continue;
        }
        if (is_ident_start(c)) {
            std::size_t j = i + 1;
            while (j < src.size() && is_ident_char(src[j])) ++j;
            std::string word = src.substr(i, j - i);
            const std::string U = up(word);
            if (U == "AND") out.push_back({Tok::And, word});
            else if (U == "OR") out.push_back({Tok::Or, word});
            else if (U == "NOT") out.push_back({Tok::Not, word});
            else out.push_back({Tok::Ident, word});
            i = j;
            continue;
        }
        break;
    }
    out.push_back({Tok::End, ""});
    return out;
}

struct ScalarValue {
    enum Kind { K_Null, K_Number, K_String, K_Bool } kind = K_Null;
    double number = 0.0;
    std::string text;
    bool tf = false;
};

static ScalarValue make_number(double v) { ScalarValue r; r.kind = ScalarValue::K_Number; r.number = v; return r; }
static ScalarValue make_string(std::string s) { ScalarValue r; r.kind = ScalarValue::K_String; r.text = std::move(s); return r; }
static ScalarValue make_bool(bool v) { ScalarValue r; r.kind = ScalarValue::K_Bool; r.tf = v; return r; }

static bool looks_like_date8(const std::string& s) {
    if (s.size() != 8) return false;
    for (char c : s) if (!std::isdigit(static_cast<unsigned char>(c))) return false;
    return true;
}

static bool parse_double_strict(const std::string& s, double& out) {
    const std::string t = trim(s);
    if (t.empty()) return false;
    char* end = nullptr;
    const double v = std::strtod(t.c_str(), &end);
    if (end == t.c_str()) return false;
    if (*end != '\0') return false;
    if (!std::isfinite(v)) return false;
    out = v;
    return true;
}

static bool as_number(const ScalarValue& v, double& out) {
    if (v.kind == ScalarValue::K_Number) { out = v.number; return true; }
    if (v.kind == ScalarValue::K_Bool) { out = v.tf ? 1.0 : 0.0; return true; }
    if (v.kind == ScalarValue::K_String) return parse_double_strict(v.text, out);
    return false;
}

static bool as_bool(const ScalarValue& v, bool& out) {
    if (v.kind == ScalarValue::K_Bool) { out = v.tf; return true; }
    if (v.kind == ScalarValue::K_Number) { out = (std::fabs(v.number) > 1e-12); return true; }
    if (v.kind == ScalarValue::K_String) {
        const std::string U = up(trim(v.text));
        if (U == ".T." || U == "T" || U == "TRUE" || U == "1") { out = true; return true; }
        if (U == ".F." || U == "F" || U == "FALSE" || U == "0" || U.empty()) { out = false; return true; }
    }
    return false;
}

static std::string scalar_to_string(const ScalarValue& v) {
    switch (v.kind) {
        case ScalarValue::K_String: return v.text;
        case ScalarValue::K_Number: {
            const double iv = std::floor(v.number);
            if (std::fabs(v.number - iv) < 1e-9) {
                std::ostringstream o; o << static_cast<long long>(iv); return o.str();
            }
            std::ostringstream o; o << std::fixed << std::setprecision(10) << v.number;
            std::string s = o.str();
            while (!s.empty() && s.find('.') != std::string::npos && s.back() == '0') s.pop_back();
            if (!s.empty() && s.back() == '.') s.pop_back();
            return s.empty() ? "0" : s;
        }
        case ScalarValue::K_Bool: return v.tf ? ".T." : ".F.";
        default: return {};
    }
}

static bool call_date_builtin(const std::string& name_upper, const std::vector<std::string>& argv, std::string& out) {
    const auto* specs = dottalk::expr::date_fn_specs();
    const std::size_t n = dottalk::expr::date_fn_specs_count();
    for (std::size_t i = 0; i < n; ++i) {
        const auto& s = specs[i];
        if (name_upper == s.name) {
            const int argc = static_cast<int>(argv.size());
            if (argc < spec_min_args(s) || argc > spec_max_args(s)) return false;
            try { out = s.fn(argv); return true; } catch (...) { return false; }
        }
    }
    return false;
}

static bool call_string_builtin(const std::string& name_upper, const std::vector<std::string>& argv, std::string& out) {
    const auto* specs = dottalk::expr::string_fn_specs();
    const std::size_t n = dottalk::expr::string_fn_specs_count();
    for (std::size_t i = 0; i < n; ++i) {
        const auto& s = specs[i];
        if (name_upper == s.name) {
            const int argc = static_cast<int>(argv.size());
            if (argc < spec_min_args(s) || argc > spec_max_args(s)) return false;
            try { out = s.fn(argv); return true; } catch (...) { return false; }
        }
    }
    return false;
}

class ValueParser {
public:
    ValueParser(xbase::DbArea* A, const std::vector<Tok>& t) : area_(A), toks_(t) {}
    bool parse_expr(ScalarValue& out) { return parse_or(out); }
    bool at_end() const { return peek().kind == Tok::End; }
private:
    xbase::DbArea* area_;
    const std::vector<Tok>& toks_;
    std::size_t pos_ = 0;
    const Tok& peek() const { return toks_[pos_]; }
    bool accept(Tok::Kind k) { if (peek().kind == k) { ++pos_; return true; } return false; }
    bool expect(Tok::Kind k) { return accept(k); }

    bool parse_or(ScalarValue& out) {
        if (!parse_and(out)) return false;
        while (accept(Tok::Or)) {
            ScalarValue rhs; if (!parse_and(rhs)) return false;
            bool a=false,b=false; if (!as_bool(out,a)||!as_bool(rhs,b)) return false;
            out = make_bool(a||b);
        }
        return true;
    }
    bool parse_and(ScalarValue& out) {
        if (!parse_compare(out)) return false;
        while (accept(Tok::And)) {
            ScalarValue rhs; if (!parse_compare(rhs)) return false;
            bool a=false,b=false; if (!as_bool(out,a)||!as_bool(rhs,b)) return false;
            out = make_bool(a&&b);
        }
        return true;
    }
    bool parse_compare(ScalarValue& out) {
        if (!parse_add(out)) return false;
        for (;;) {
            Tok::Kind op = peek().kind;
            if (!(op==Tok::Eq||op==Tok::Ne||op==Tok::Lt||op==Tok::Le||op==Tok::Gt||op==Tok::Ge)) return true;
            ++pos_;
            ScalarValue rhs; if (!parse_add(rhs)) return false;
            if (out.kind==ScalarValue::K_String && rhs.kind==ScalarValue::K_String && looks_like_date8(out.text) && looks_like_date8(rhs.text)) {
                int cmp = out.text.compare(rhs.text);
                bool r=false;
                switch(op){case Tok::Eq:r=(cmp==0);break;case Tok::Ne:r=(cmp!=0);break;case Tok::Lt:r=(cmp<0);break;case Tok::Le:r=(cmp<=0);break;case Tok::Gt:r=(cmp>0);break;case Tok::Ge:r=(cmp>=0);break;default:return false;}
                out = make_bool(r); continue;
            }
            double a=0,b=0;
            if (as_number(out,a) && as_number(rhs,b)) {
                bool r=false;
                switch(op){case Tok::Eq:r=(std::fabs(a-b)<1e-9);break;case Tok::Ne:r=!(std::fabs(a-b)<1e-9);break;case Tok::Lt:r=(a<b);break;case Tok::Le:r=(a<=b);break;case Tok::Gt:r=(a>b);break;case Tok::Ge:r=(a>=b);break;default:return false;}
                out = make_bool(r); continue;
            }
            std::string sa=scalar_to_string(out), sb=scalar_to_string(rhs); int cmp=sa.compare(sb); bool r=false;
            switch(op){case Tok::Eq:r=(cmp==0);break;case Tok::Ne:r=(cmp!=0);break;case Tok::Lt:r=(cmp<0);break;case Tok::Le:r=(cmp<=0);break;case Tok::Gt:r=(cmp>0);break;case Tok::Ge:r=(cmp>=0);break;default:return false;}
            out = make_bool(r);
        }
    }
    bool parse_add(ScalarValue& out) {
        if (!parse_mul(out)) return false;
        for (;;) {
            if (accept(Tok::Plus)) {
                ScalarValue rhs; if (!parse_mul(rhs)) return false;
                if (out.kind==ScalarValue::K_String && looks_like_date8(out.text)) {
                    double days=0; if (!as_number(rhs,days)) return false;
                    std::string added; if (!call_date_builtin("DATEADD", {out.text, std::to_string((int)std::llround(days))}, added)) return false;
                    out = make_string(added); continue;
                }
                double a=0,b=0; if (as_number(out,a) && as_number(rhs,b)) out = make_number(a+b);
                else out = make_string(scalar_to_string(out)+scalar_to_string(rhs));
                continue;
            }
            if (accept(Tok::Minus)) {
                ScalarValue rhs; if (!parse_mul(rhs)) return false;
                if (out.kind==ScalarValue::K_String && looks_like_date8(out.text)) {
                    double days=0;
                    if (as_number(rhs,days)) {
                        std::string added; if (!call_date_builtin("DATEADD", {out.text, std::to_string(-(int)std::llround(days))}, added)) return false;
                        out = make_string(added); continue;
                    }
                }
                if (out.kind==ScalarValue::K_String && rhs.kind==ScalarValue::K_String && looks_like_date8(out.text) && looks_like_date8(rhs.text)) {
                    std::string diff; if (!call_date_builtin("DATEDIFF", {out.text, rhs.text}, diff)) return false;
                    double dv=0; if (!parse_double_strict(diff,dv)) return false;
                    out = make_number(dv); continue;
                }
                double a=0,b=0; if (!as_number(out,a) || !as_number(rhs,b)) return false; out = make_number(a-b); continue;
            }
            return true;
        }
    }
    bool parse_mul(ScalarValue& out) {
        if (!parse_unary(out)) return false;
        for (;;) {
            if (accept(Tok::Star)) { ScalarValue rhs; if (!parse_unary(rhs)) return false; double a=0,b=0; if(!as_number(out,a)||!as_number(rhs,b)) return false; out=make_number(a*b); continue; }
            if (accept(Tok::Slash)) { ScalarValue rhs; if (!parse_unary(rhs)) return false; double a=0,b=0; if(!as_number(out,a)||!as_number(rhs,b)) return false; if (std::fabs(b)<1e-12) return false; out=make_number(a/b); continue; }
            return true;
        }
    }
    bool parse_unary(ScalarValue& out) {
        if (accept(Tok::Not)) { if(!parse_unary(out)) return false; bool b=false; if(!as_bool(out,b)) return false; out=make_bool(!b); return true; }
        if (accept(Tok::Plus)) { if(!parse_unary(out)) return false; double d=0; if(!as_number(out,d)) return false; out=make_number(+d); return true; }
        if (accept(Tok::Minus)) { if(!parse_unary(out)) return false; double d=0; if(!as_number(out,d)) return false; out=make_number(-d); return true; }
        return parse_primary(out);
    }
    bool parse_primary(ScalarValue& out) {
        const Tok& t = peek();
        if (t.kind == Tok::String) { out = make_string(t.text); ++pos_; return true; }
        if (t.kind == Tok::Number) { double d=0; if(!parse_double_strict(t.text,d)) return false; out=make_number(d); ++pos_; return true; }
        if (accept(Tok::LParen)) { if(!parse_expr(out)) return false; if(!expect(Tok::RParen)) return false; return true; }
        if (t.kind == Tok::Ident) {
            std::string ident=t.text; ++pos_;
            if (accept(Tok::LParen)) {
                std::vector<std::string> args;
                if (!accept(Tok::RParen)) {
                    while (true) {
                        ScalarValue av; if(!parse_expr(av)) return false; args.push_back(scalar_to_string(av));
                        if (accept(Tok::Comma)) continue; if(!expect(Tok::RParen)) return false; break;
                    }
                }
                const std::string fn = up(ident); std::string result;
                if (call_string_builtin(fn,args,result)) {
                    const std::string U=up(result);
                    if (fn=="LIKE" || fn=="$") out=make_bool(U==".T."||U=="T"||U=="TRUE"||U=="1");
                    else out=make_string(result);
                    return true;
                }
                if (call_date_builtin(fn,args,result)) { out=make_string(result); return true; }
                return false;
            }
            if (area_ && area_->isOpen()) {
                int fld = field_index_ci(*area_, ident); if (fld > 0) { out = make_string(get_logical_field_text(*area_, fld)); return true; }
            }
            const std::string U = up(ident);
            if (U=="TRUE"||U=="T") { out=make_bool(true); return true; }
            if (U=="FALSE"||U=="F") { out=make_bool(false); return true; }
            out=make_string(ident); return true;
        }
        return false;
    }
};

static dottalk::expr::EvalValue scalar_to_eval(const ScalarValue& v) {
    dottalk::expr::EvalValue ev;
    switch (v.kind) {
        case ScalarValue::K_Bool:
            ev.kind = dottalk::expr::EvalValue::K_Bool;
            ev.tf = v.tf;
            break;
        case ScalarValue::K_Number:
            ev.kind = dottalk::expr::EvalValue::K_Number;
            ev.number = v.number;
            break;
        case ScalarValue::K_String:
            if (looks_like_date8(v.text)) {
                ev.kind = dottalk::expr::EvalValue::K_Date;
                ev.date8 = static_cast<std::int32_t>(std::strtol(v.text.c_str(), nullptr, 10));
            } else {
                ev.kind = dottalk::expr::EvalValue::K_String;
                ev.text = v.text;
            }
            break;
        default:
            ev.kind = dottalk::expr::EvalValue::K_None;
            break;
    }
    return ev;
}

static bool eval_scalar_expr(xbase::DbArea* A, const std::string& exprText, dottalk::expr::EvalValue& out) {
    auto toks = lex_value_expr(exprText);
    ValueParser p(A, toks);
    ScalarValue v;
    if (!p.parse_expr(v)) return false;
    if (!p.at_end()) return false;
    out = scalar_to_eval(v);
    return true;
}

} // namespace

namespace dottalk::expr {

EvalValue eval_rhs(xbase::DbArea* areaOrNull,
                   const std::string& exprText,
                   std::string* errOut)
{
    if (errOut) errOut->clear();
    const std::string expr = trim(exprText);
    if (expr.empty()) {
        if (errOut) *errOut = "empty expression";
        return {};
    }

    if (areaOrNull && areaOrNull->isOpen()) {
        // Prefer the common scalar parser first. It already understands:
        //   - field references
        //   - date builtins
        //   - date8 + int / date8 - int / date8 - date8
        // This avoids generic eval_any() flattening DBF D fields into plain numbers.
        EvalValue ev;
        if (eval_scalar_expr(areaOrNull, expr, ev)) return ev;

        std::string err;
        ev = eval_any(*areaOrNull, expr, &err);
        if (ev.kind != EvalValue::K_None) return ev;

        if (errOut) *errOut = err.empty() ? "evaluation failed" : err;
        return {};
    }

    EvalValue ev;
    if (eval_scalar_expr(nullptr, expr, ev)) return ev;
    if (errOut) *errOut = "scalar evaluation failed";
    return {};
}

} // namespace dottalk::expr
