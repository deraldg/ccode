#include "dottalk/expr/lexer.hpp"
#include <cctype>

using namespace dottalk::expr;

static bool is_ident_start(char c) {
  return std::isalpha(static_cast<unsigned char>(c)) || c=='_';
}
static bool is_ident_part(char c) {
  return std::isalnum(static_cast<unsigned char>(c)) || c=='_' ;
}

Lexer::Lexer(std::string src): m_src(std::move(src)) {}

void Lexer::skipSpace() {
  while (!eof()) {
    char c = cur();
    if (std::isspace(static_cast<unsigned char>(c))) { ++m_i; continue; }
    if (c=='.' && std::isalpha(static_cast<unsigned char>(peekch()))) break;
    break;
  }
}

Token Lexer::readIdent() {
  size_t start = m_i++;
  while (!eof() && is_ident_part(cur())) ++m_i;
  Token t;
  t.kind = TokKind::Ident;
  t.lexeme = std::string(m_src.substr(start, m_i - start));
  std::string U;
  U.reserve(t.lexeme.size());
  for (char ch: t.lexeme) U.push_back(static_cast<char>(std::toupper(static_cast<unsigned char>(ch))));
  if (U=="AND") t.kind = TokKind::KW_AND;
  else if (U=="OR") t.kind = TokKind::KW_OR;
  else if (U=="NOT") t.kind = TokKind::KW_NOT;
  return t;
}

Token Lexer::readNumber() {
  size_t start = m_i;
  while (!eof() && std::isdigit(static_cast<unsigned char>(cur()))) ++m_i;
  if (!eof() && cur()=='.') {
    ++m_i;
    while (!eof() && std::isdigit(static_cast<unsigned char>(cur()))) ++m_i;
  }
  Token t;
  t.kind = TokKind::Number;
  t.lexeme = std::string(m_src.substr(start, m_i - start));
  t.number = std::strtod(t.lexeme.c_str(), nullptr);
  return t;
}

Token Lexer::readString() {
  std::string out;
  while (!eof()) {
    char c = cur(); ++m_i;
    if (c=='"') break;
    if (c=='\\' && !eof()) {
      char n = cur(); ++m_i;
      if (n=='"' || n=='\\') out.push_back(n);
      else { out.push_back('\\'); out.push_back(n); }
    } else {
      out.push_back(c);
    }
  }
  Token t;
  t.kind = TokKind::String;
  t.lexeme = std::move(out);
  return t;
}

const Token& Lexer::peek() {
  if (!m_hasLook) { m_look = next(); m_hasLook = true; }
  return m_look;
}
void Lexer::consume() { m_hasLook = false; }

Token Lexer::next() {
  skipSpace();
  if (eof()) return Token{TokKind::End, ""};

  char c = cur();
  if (c=='.') {
    size_t start = m_i++;
    while (!eof() && cur()!='.') ++m_i;
    if (!eof() && cur()=='.') ++m_i;
    std::string s = m_src.substr(start, m_i-start);
    std::string U;
    for (char ch: s) U.push_back(static_cast<char>(std::toupper(static_cast<unsigned char>(ch))));
    if (U==".AND.") return Token{TokKind::KW_AND, s};
    if (U==".OR.")  return Token{TokKind::KW_OR,  s};
    if (U==".NOT.") return Token{TokKind::KW_NOT, s};
    return Token{TokKind::Ident, s};
  }

  if (is_ident_start(c)) return readIdent();
  if (std::isdigit(static_cast<unsigned char>(c))) return readNumber();
  if (c=='"') { ++m_i; return readString(); }

  if (c=='(') { ++m_i; return Token{TokKind::LParen, "("}; }
  if (c==')') { ++m_i; return Token{TokKind::RParen, ")"}; }
  if (c=='=' && peekch()=='=') { m_i+=2; return Token{TokKind::EqEq, "=="}; }
  if (c=='=') { ++m_i; return Token{TokKind::Eq, "="}; }
  if (c=='!' && peekch()=='=') { m_i+=2; return Token{TokKind::Ne, "!="}; }
  if (c=='<' && peekch()=='=') { m_i+=2; return Token{TokKind::Le, "<="}; }
  if (c=='>' && peekch()=='=') { m_i+=2; return Token{TokKind::Ge, ">="}; }
  if (c=='<') { ++m_i; return Token{TokKind::Lt, "<"}; }
  if (c=='>') { ++m_i; return Token{TokKind::Gt, ">"}; }

  ++m_i;
  return Token{TokKind::End, ""};
}
