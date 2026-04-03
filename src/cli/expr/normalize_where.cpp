#include "xbase.hpp"
#include "textio.hpp"

#include <string>
#include <vector>
#include <cctype>
#include <sstream>

// --- reuse patterns from predicates.cpp ---

static bool looks_numeric_literal(const std::string& s) {
    if (s.empty()) return false;
    bool saw_digit = false, saw_dot = false;
    size_t i = 0;

    if (s[i] == '+' || s[i] == '-') ++i;

    for (; i < s.size(); ++i) {
        unsigned char ch = (unsigned char)s[i];
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

static bool is_identifier(const std::string& s) {
    if (s.empty()) return false;
    if (!std::isalpha((unsigned char)s[0]) && s[0] != '_') return false;
    for (char c : s) {
        if (!std::isalnum((unsigned char)c) && c != '_') return false;
    }
    return true;
}

static std::string upper_copy(std::string s) {
    for (char& c : s) c = (char)std::toupper((unsigned char)c);
    return s;
}

static bool is_reserved_literal(const std::string& tok) {
    const std::string u = upper_copy(tok);
    return (u == "TRUE" || u == "FALSE" || u == "NULL" ||
            u == ".T." || u == ".F.");
}

static bool is_known_field_ci(const xbase::DbArea& a, const std::string& name) {
    const auto& f = a.fields();
    const std::string want = upper_copy(name);
    for (const auto& fld : f) {
        if (upper_copy(fld.name) == want) return true;
    }
    return false;
}

static std::string escape_string(const std::string& s) {
    std::string out;
    out.reserve(s.size() + 4);
    for (char c : s) {
        if (c == '\\' || c == '"') out.push_back('\\');
        out.push_back(c);
    }
    return out;
}

// --- simple tokenizer (reuse-light, safe) ---
static std::vector<std::string> tokenize(const std::string& expr) {
    std::vector<std::string> t;
    std::string cur;

    for (size_t i = 0; i < expr.size(); ++i) {
        char c = expr[i];

        if (std::isspace((unsigned char)c)) {
            if (!cur.empty()) { t.push_back(cur); cur.clear(); }
            continue;
        }

        // operators
        if (c == '=' || c == '<' || c == '>' || c == '!') {
            if (!cur.empty()) { t.push_back(cur); cur.clear(); }

            std::string op(1, c);
            if (i + 1 < expr.size()) {
                char n = expr[i + 1];
                if ((c == '<' && n == '>') ||
                    (c == '<' && n == '=') ||
                    (c == '>' && n == '=') ||
                    (c == '!' && n == '=') ||
                    (c == '=' && n == '=')) {
                    op.push_back(n);
                    ++i;
                }
            }
            t.push_back(op);
            continue;
        }

        if (c == '"' || c == '\'') {
            if (!cur.empty()) { t.push_back(cur); cur.clear(); }

            char q = c;
            std::string s;
            s.push_back(q);
            ++i;
            while (i < expr.size()) {
                char d = expr[i];
                s.push_back(d);
                if (d == q) break;
                if (d == '\\' && i + 1 < expr.size()) {
                    s.push_back(expr[++i]);
                }
                ++i;
            }
            t.push_back(s);
            continue;
        }

        cur.push_back(c);
    }

    if (!cur.empty()) t.push_back(cur);
    return t;
}

static bool is_cmp(const std::string& t) {
    return (t == "=" || t == "==" || t == "!=" || t == "<>" ||
            t == "<" || t == "<=" || t == ">" || t == ">=");
}

// --- MAIN FUNCTION ---

std::string normalize_unquoted_rhs_literals(xbase::DbArea& a,
                                            const std::string& expr)
{
    auto toks = tokenize(expr);

    for (size_t i = 0; i + 1 < toks.size(); ++i) {
        if (!is_cmp(toks[i])) continue;

        std::string& rhs = toks[i + 1];

        if (rhs.empty()) continue;

        // already quoted
        if (rhs.front() == '"' || rhs.front() == '\'') continue;

        // numeric
        if (looks_numeric_literal(rhs)) continue;

        // reserved literal
        if (is_reserved_literal(rhs)) continue;

        // field name
        if (is_known_field_ci(a, rhs)) continue;

        // identifier only
        if (!is_identifier(rhs)) continue;

        // function call (lookahead "(")
        if (i + 2 < toks.size() && toks[i + 2] == "(") continue;

        // rewrite
        rhs = "\"" + escape_string(rhs) + "\"";
    }

    // rebuild
    std::ostringstream out;
    for (size_t i = 0; i < toks.size(); ++i) {
        if (i) out << ' ';
        out << toks[i];
    }
    return out.str();
}