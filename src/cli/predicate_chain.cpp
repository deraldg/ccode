
// src/cli/predicate_chain.cpp
#include "predicate_chain.hpp"
#include "value_normalize.hpp"
#include "xbase_field_getters.hpp"

#include <algorithm>
#include <cctype>
#include <optional>
#include <string>
#include <sstream>
#include <vector>

using util::normalize_for_compare;

namespace {

struct Tok {
    enum Kind { ID, OP, STR, NUM, AND, OR, END, BAD } k{BAD};
    std::string text;
};

static inline bool is_ident_start(unsigned char c) {
    return std::isalpha(c) || c=='_' || c=='$';
}
static inline bool is_ident_body(unsigned char c) {
    return std::isalnum(c) || c=='_' || c=='$';
}

struct Lexer {
    std::string s;
    size_t i{0};

    explicit Lexer(std::string src): s(std::move(src)) {}

    void skipws() {
        while (i < s.size() && std::isspace((unsigned char)s[i])) ++i;
    }

    Tok next() {
        skipws();
        if (i >= s.size()) return Tok{Tok::END, ""};

        unsigned char c = (unsigned char)s[i];

        // String literal (single or double quotes)
        if (c=='\'' || c=='"') {
            char q = (char)c; ++i;
            std::string v;
            while (i < s.size() && s[i] != q) {
                v.push_back(s[i++]);
            }
            if (i < s.size() && s[i]==q) ++i; // consume closing quote
            return Tok{Tok::STR, v};
        }

        // Relational operator
        if (c=='=' || c=='!' || c=='<' || c=='>') {
            // try two-char ops first
            if (i+1 < s.size()) {
                std::string two = s.substr(i,2);
                if (two=="==" || two=="<>" || two=="!=" || two==">=" || two=="<=") {
                    i += 2;
                    return Tok{Tok::OP, two};
                }
            }
            // single-char op
            i += 1;
            return Tok{Tok::OP, std::string(1,(char)c)};
        }

        // Identifier (field name or AND/OR)
        if (is_ident_start(c)) {
            size_t start = i++;
            while (i < s.size() && is_ident_body((unsigned char)s[i])) ++i;
            std::string id = s.substr(start, i-start);
            std::string up = id;
            std::transform(up.begin(), up.end(), up.begin(), [](unsigned char ch){ return (char)std::toupper(ch); });
            if (up=="AND") return Tok{Tok::AND, id};
            if (up=="OR")  return Tok{Tok::OR, id};
            return Tok{Tok::ID, id};
        }

        // Bare numeric token (we'll still normalize using field type later)
        if (std::isdigit(c)) {
            size_t start = i++;
            while (i < s.size() && (std::isdigit((unsigned char)s[i]) || s[i]==',' || s[i]=='.')) ++i;
            return Tok{Tok::NUM, s.substr(start, i-start)};
        }

        // Bare token (for unquoted text without spaces)
        if (!std::isspace(c)) {
            size_t start = i++;
            while (i < s.size() && !std::isspace((unsigned char)s[i])) ++i;
            return Tok{Tok::STR, s.substr(start, i-start)};
        }

        return Tok{Tok::BAD, ""};
    }
};

static inline std::string upper(std::string s) {
    std::transform(s.begin(), s.end(), s.begin(), [](unsigned char c){ return (char)std::toupper(c); });
    return s;
}

static int field_index_ci(const xbase::DbArea& a, const std::string& name) {
    std::string needle = name;
    while (!needle.empty() && std::isspace((unsigned char)needle.back())) needle.pop_back();
    const auto& Fs = a.fields();
    for (int i = 0; i < (int)Fs.size(); ++i) {
        std::string hay = Fs[(size_t)i].name;
        while (!hay.empty() && std::isspace((unsigned char)hay.back())) hay.pop_back();
        std::string H = upper(hay), N = upper(needle);
        if (H == N) return i + 1; // 1-based
    }
    return 0;
}

static inline bool is_relop(const std::string& opU) {
    return opU=="=" || opU=="==" || opU=="<>" || opU=="!=" ||
           opU==">" || opU=="<"  || opU==">=" || opU=="<=";
}

// Compare two canonical strings according to field type
static bool compare_canonical(char ftype, const std::string& L, const std::string& opU, const std::string& R) {
    auto cmp_str = [](const std::string& a, const std::string& b)->int {
        if (a==b) return 0;
        return (a<b) ? -1 : +1;
    };

    int rel = 0;
    if (ftype=='N') {
        long double l=0, r=0;
        try { l = std::stold(L); r = std::stold(R); }
        catch(...) { return false; }
        if (l < r) rel = -1; else if (l > r) rel = +1; else rel = 0;
    } else if (ftype=='D' || ftype=='L') {
        rel = cmp_str(L, R);
    } else { // 'C' & others: case-insensitive
        std::string a=L, b=R;
        std::transform(a.begin(), a.end(), a.begin(), [](unsigned char c){ return (char)std::toupper(c); });
        std::transform(b.begin(), b.end(), b.begin(), [](unsigned char c){ return (char)std::toupper(c); });
        rel = cmp_str(a, b);
    }

    if (opU=="=" || opU=="==") return rel == 0;
    if (opU=="<>" || opU=="!=") return rel != 0;
    if (opU==">")  return rel >  0;
    if (opU==">=") return rel >= 0;
    if (opU=="<")  return rel <  0;
    if (opU=="<=") return rel <= 0;
    return false;
}

struct Cond { std::string field; std::string opU; std::string valueRaw; };

// Parse a single condition: FIELD OP VALUE
static bool parse_cond(Lexer& lex, Cond& out) {
    Tok t1 = lex.next();
    if (t1.k != Tok::ID) return false;
    Tok op = lex.next();
    if (op.k != Tok::OP) return false;
    Tok v  = lex.next();
    if (v.k!=Tok::STR && v.k!=Tok::NUM && v.k!=Tok::ID) return false;
    out.field = t1.text;
    out.opU   = upper(op.text);
    out.valueRaw = v.text;
    return true;
}

// Evaluate a single condition
static bool eval_cond_once(const xbase::DbArea& A, const Cond& c) {
    if (!is_relop(c.opU)) return false;
    const int fidx = field_index_ci(A, c.field);
    if (fidx <= 0) return false;

    const auto& F = A.fields()[(size_t)(fidx-1)];
    const char ftype = (char)std::toupper((unsigned char)F.type);
    const int  flen  = (int)F.length;
    const int  fdec  = (int)F.decimals;

    std::string leftRaw  = A.get(fidx);
    auto leftCanon  = normalize_for_compare(ftype, flen, fdec, leftRaw);
    auto rightCanon = normalize_for_compare(ftype, flen, fdec, c.valueRaw);
    if (!leftCanon || !rightCanon) return false;

    return compare_canonical(ftype, *leftCanon, c.opU, *rightCanon);
}

} // namespace

bool try_eval_predicate_chain(const xbase::DbArea& area,
                              const std::string& expr,
                              bool& out_result,
                              std::string* error_msg)
{
    (void)error_msg;
    Lexer lex(expr);

    // Parse first condition
    Cond first{};
    if (!parse_cond(lex, first)) {
        return false; // not a chain
    }
    bool acc = eval_cond_once(area, first);

    // Parse zero or more (AND|OR cond)
    while (true) {
        Tok t = lex.next();
        if (t.k == Tok::AND) {
            Cond c{};
            if (!parse_cond(lex, c)) return false; // malformed after AND
            bool v = eval_cond_once(area, c);
            acc = acc && v;
            continue;
        } else if (t.k == Tok::OR) {
            Cond c{};
            if (!parse_cond(lex, c)) return false; // malformed after OR
            bool v = eval_cond_once(area, c);
            acc = acc || v;
            continue;
        } else if (t.k == Tok::END) {
            break;
        } else {
            // Unexpected token where END/AND/OR expected ? not a recognized chain
            return false;
        }
    }

    out_result = acc;
    return true; // handled
}



