#pragma once
#include <string>
#include "cli/expr/token.hpp"

namespace dottalk { namespace expr {

class Lexer {
public:
  explicit Lexer(std::string src);
  Token next();
  const Token& peek();
  void consume();

private:
  std::string m_src;
  size_t m_i{0};
  Token m_look;
  bool m_hasLook{false};

  void skipSpace();
  bool eof() const { return m_i >= m_src.size(); }
  char cur() const { return eof()? '\0' : m_src[m_i]; }
  char peekch(size_t k=1) const { return (m_i+k<m_src.size())? m_src[m_i+k] : '\0'; }
  Token readIdent();
  Token readNumber();
  Token readString();
};

}} // namespace



