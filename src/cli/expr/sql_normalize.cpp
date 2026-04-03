// src/cli/expr/sql_normalize.cpp
#include "sql_normalize.hpp"

#include <algorithm>
#include <cctype>
#include <sstream>
#include <string>
#include <string_view>
#include <vector>

namespace sqlnorm {

namespace {

// ------------------------------- Tokenizer ---------------------------------

enum class Tok {
    Identifier,   // field names, bare values (no spaces)
    Number,       // 123, 45.67
    String,       // 'abc' or "abc"
    LParen,       // (
    RParen,       // )
    OpEQ,         // =
    OpNE,         // <> or !=
    OpGT,         // >
    OpLT,         // <
    OpGE,         // >=
    OpLE,         // <=
    And,          // AND
    Or,           // OR
    Not,          // NOT
    End
};

struct Token {
    Tok kind{};
    std::string text;   // original text (for Identifier/String/Number)
};

static inline bool is_ident_start(char c) {
    return std::isalpha(static_cast<unsigned char>(c)) || c == '_';
}
static inline bool is_ident_char(char c) {
    return std::isalnum(static_cast<unsigned char>(c)) || c == '_';
}
static inline bool is_space(char c) {
    return c == ' ' || c == '\t' || c == '\r' || c == '\n';
}

static std::string up(std::string s) {
    std::transform(s.begin(), s.end(), s.begin(),
                   [](unsigned char c){ return static_cast<char>(std::toupper(c)); });
    return s;
}

static Token make_kw_or_ident(std::string word) {
    std::string U = up(word);
    if (U == "AND") return {Tok::And, U};
    if (U == "OR")  return {Tok::Or,  U};
    if (U == "NOT") return {Tok::Not, U};
    return {Tok::Identifier, word};
}

static std::vector<Token> tokenize(const std::string& s) {
    std::vector<Token> out;
    size_t i = 0, n = s.size();

    auto push = [&](Tok k, std::string v = std::string{}) {
        out.push_back(Token{k, std::move(v)});
    };

    while (i < n) {
        char c = s[i];
        if (is_space(c)) { ++i; continue; }

        // String literal (single or double quotes). Support doubled quotes as escape.
        if (c == '\'' || c == '"') {
            char quote = c;
            std::string buf;
            buf.push_back(quote);
            ++i;
            while (i < n) {
                char d = s[i++];
                buf.push_back(d);
                if (d == quote) {
                    if (i < n && s[i] == quote) { // doubled quote -> escaped
                        buf.push_back(s[i++]);
                        continue;
                    }
                    break; // closing quote
                }
            }
            push(Tok::String, buf);
            continue;
        }

        // Identifier / bare value
        if (is_ident_start(c)) {
            size_t j = i + 1;
            while (j < n && is_ident_char(s[j])) ++j;
            std::string word = s.substr(i, j - i);
            out.push_back(make_kw_or_ident(word));
            i = j;
            continue;
        }

        // Number (digits + optional single dot)
        if (std::isdigit(static_cast<unsigned char>(c))) {
            size_t j = i + 1;
            bool dot = false;
            while (j < n) {
                char d = s[j];
                if (std::isdigit(static_cast<unsigned char>(d))) { ++j; continue; }
                if (d == '.' && !dot) { dot = true; ++j; continue; }
                break;
            }
            push(Tok::Number, s.substr(i, j - i));
            i = j;
            continue;
        }

        // Two-char operators
        if (i + 1 < n) {
            char d = s[i+1];
            if (c == '<' && d == '>') { push(Tok::OpNE); i += 2; continue; }
            if (c == '!' && d == '=') { push(Tok::OpNE); i += 2; continue; }
            if (c == '<' && d == '=') { push(Tok::OpLE); i += 2; continue; }
            if (c == '>' && d == '=') { push(Tok::OpGE); i += 2; continue; }
        }

        // Single-char operators and parens
        if (c == '=') { push(Tok::OpEQ); ++i; continue; }
        if (c == '>') { push(Tok::OpGT); ++i; continue; }
        if (c == '<') { push(Tok::OpLT); ++i; continue; }
        if (c == '(') { push(Tok::LParen); ++i; continue; }
        if (c == ')') { push(Tok::RParen); ++i; continue; }

        // Unknown char ? skip it.
        ++i;
    }

    out.push_back(Token{Tok::End, {}});
    return out;
}

// ---------------------------- Normalization -------------------------------

static bool is_value_token(const Token& t) {
    return t.kind == Tok::String || t.kind == Tok::Number || t.kind == Tok::Identifier;
}
static std::string render_ident_field(const std::string& ident) {
    return up(ident);
}

// normalize any quoted string to **double-quoted**, uppercased inner text,
// with internal " escaped by doubling.
static std::string normalize_quoted(std::string qtext) {
    if (qtext.empty()) return "\"\"";
    char q = qtext.front();
    if (q != '\'' && q != '"') {
        std::string U = up(qtext);
        return "\"" + U + "\"";
    }
    // strip outer quotes
    std::string inner = qtext.substr(1, qtext.size() >= 2 ? qtext.size()-2 : 0);
    // collapse doubled quotes of same kind
    std::string tmp; tmp.reserve(inner.size());
    for (size_t i=0;i<inner.size();) {
        char c = inner[i++];
        if (c == q && i < inner.size() && inner[i] == q) { tmp.push_back(q); ++i; continue; }
        tmp.push_back(c);
    }
    std::string U = up(tmp);
    // rewrap with **double** quotes, escape " by doubling
    std::string out; out.reserve(U.size()+2);
    out.push_back('"');
    for (char c : U) {
        if (c == '"') out.push_back('"'); // double it
        out.push_back(c);
    }
    out.push_back('"');
    return out;
}

static std::string render_value(const Token& t) {
    switch (t.kind) {
        case Tok::String:     return normalize_quoted(t.text);
        case Tok::Number:     return t.text;
        case Tok::Identifier: {
            // bare identifiers on RHS -> treat as strings (double-quoted + upcase)
            std::string U = up(t.text);
            std::string out; out.reserve(U.size()+2);
            out.push_back('"'); out += U; out.push_back('"');
            return out;
        }
        default: return t.text;
    }
}
static std::string render_op(const Token& t) {
    switch (t.kind) {
        case Tok::OpEQ: return " = ";
        case Tok::OpNE: return " <> ";
        case Tok::OpGT: return " > ";
        case Tok::OpLT: return " < ";
        case Tok::OpGE: return " >= ";
        case Tok::OpLE: return " <= ";
        default:        return " ";
    }
}

static std::string consume_simple_expr(const std::vector<Token>& toks, size_t& i);

static std::string normalize_tokens(const std::vector<Token>& toks) {
    std::ostringstream out;
    size_t i = 0;

    auto peek = [&](size_t k = 0)->const Token& { return toks[i + k]; };
    auto eat  = [&](){ return toks[i++]; };

    while (true) {
        const Token& t = peek();
        if (t.kind == Tok::End) break;

        switch (t.kind) {
            case Tok::And: out << " .AND. "; eat(); break;
            case Tok::Or:  out << " .OR. ";  eat(); break;
            case Tok::LParen: out << "("; eat(); break;
            case Tok::RParen: out << ")"; eat(); break;

            case Tok::Not: {
                eat(); // NOT
                out << ".NOT. ";
                if (peek().kind == Tok::LParen) {
                    continue; // next loop prints '('
                } else {
                    out << "(";
                    out << consume_simple_expr(toks, i);
                    out << ")";
                }
                break;
            }

            case Tok::Identifier: {
                std::string lhs = render_ident_field(t.text);
                eat(); // IDENT

                // IDENT NOT <value>  -> IDENT <> <value>
                if (peek().kind == Tok::Not) {
                    eat(); // NOT
                    const Token& valTok = peek();
                    if (is_value_token(valTok)) {
                        out << lhs << " <> " << render_value(valTok);
                        eat(); // value
                    } else {
                        out << ".NOT. (" << lhs;
                        if (peek().kind == Tok::OpEQ || peek().kind == Tok::OpNE ||
                            peek().kind == Tok::OpGT || peek().kind == Tok::OpLT ||
                            peek().kind == Tok::OpGE || peek().kind == Tok::OpLE) {
                            out << render_op(peek()); eat();
                            if (is_value_token(peek())) {
                                out << render_value(peek()); eat();
                            }
                        }
                        out << ")";
                    }
                    break;
                }

                // IDENT <op> <rhs>
                if (peek().kind == Tok::OpEQ || peek().kind == Tok::OpNE ||
                    peek().kind == Tok::OpGT || peek().kind == Tok::OpLT ||
                    peek().kind == Tok::OpGE || peek().kind == Tok::OpLE) {
                    std::string op = render_op(peek());
                    eat(); // operator
                    const Token& rhs = peek();

                    if (is_value_token(rhs)) {
                        out << lhs << op << render_value(rhs);
                        eat(); // rhs
                    } else if (rhs.kind == Tok::LParen) {
                        // IDENT <op> ( <expr> ) ? hoist first value if any
                        eat(); // '('
                        out << "(";
                        if (is_value_token(peek())) {
                            out << lhs << op << render_value(peek());
                            eat();
                        } else {
                            out << lhs << op;
                        }
                        continue; // copy inner until ')'
                    } else {
                        out << lhs; // fallback
                    }
                    break;
                }

                // Standalone identifier
                out << lhs;
                break;
            }

            case Tok::Number:
            case Tok::String:
                out << render_value(t); eat(); break;

            case Tok::OpEQ:
            case Tok::OpNE:
            case Tok::OpGT:
            case Tok::OpLT:
            case Tok::OpGE:
            case Tok::OpLE:
                out << render_op(t); eat(); break;

            default:
                eat(); break;
        }
    }

    return out.str();
}

static std::string consume_simple_expr(const std::vector<Token>& toks, size_t& i) {
    std::ostringstream out;
    auto boundary = [&](Tok k)->bool {
        return k == Tok::And || k == Tok::Or || k == Tok::RParen || k == Tok::End;
    };

    size_t j = i;
    while (!boundary(toks[j].kind)) {
        const Token& t = toks[j];
        switch (t.kind) {
            case Tok::Identifier: out << up(t.text); break;
            case Tok::String:
            case Tok::Number:     out << render_value(t); break;
            case Tok::OpEQ: out << " = ";  break;
            case Tok::OpNE: out << " <> "; break;
            case Tok::OpGT: out << " > ";  break;
            case Tok::OpLT: out << " < ";  break;
            case Tok::OpGE: out << " >= "; break;
            case Tok::OpLE: out << " <= "; break;
            case Tok::LParen: out << "("; break;
            case Tok::RParen: out << ")"; break;
            case Tok::Not:    out << ".NOT. "; break;
            case Tok::And:    out << " .AND. "; break;
            case Tok::Or:     out << " .OR. "; break;
            default: break;
        }
        ++j;
    }
    i = j;
    return out.str();
}

} // namespace

// ------------------------------- Public API --------------------------------

std::string sql_to_dottalk_where(const std::string& input) {
    auto toks = tokenize(input);
    std::string s = normalize_tokens(toks);

    // Safety: coerce any stray "==" to "=" (doesn't touch >=, <=).
    size_t pos = 0;
    while ((pos = s.find("==", pos)) != std::string::npos) {
        s.replace(pos, 2, " = ");
        pos += 3;
    }

    return s;
}

} // namespace sqlnorm



