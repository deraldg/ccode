#pragma once
#include <memory>
#include <string>
#include "cli/expr/ast.hpp"
#include "cli/expr/lexer.hpp"
#include "cli/expr/token.hpp"

namespace dottalk { namespace expr {

struct ParseError { std::string message; };

class Parser {
public:
  explicit Parser(std::string src): m_lex(std::move(src)) {}
  std::unique_ptr<Expr> parse_expr();
private:
  Lexer m_lex;
  int lbp(const Token& t) const;
  std::unique_ptr<Expr> nud(Token t);
  std::unique_ptr<Expr> led(std::unique_ptr<Expr> left, Token op);
  std::unique_ptr<Expr> expression(int min_bp);
  Token expect(TokKind k, const char* msg);
};

}} // namespace



