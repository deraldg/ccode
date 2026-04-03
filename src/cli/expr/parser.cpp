#include "cli/expr/parser.hpp"
#include "cli/expr/eval.hpp"
#include <stdexcept>

using namespace dottalk::expr;

int Parser::lbp(const Token& t) const {
  switch (t.kind) {
    case TokKind::Star:
    case TokKind::Slash: return 60; // highest infix precedence
    case TokKind::Plus:
    case TokKind::Minus: return 50;
    case TokKind::Eq:
    case TokKind::EqEq:
    case TokKind::Ne:
    case TokKind::Lt:
    case TokKind::Le:
    case TokKind::Gt:
    case TokKind::Ge: return 40;
    case TokKind::KW_AND: return 20;
    case TokKind::KW_OR:  return 10;
    default: return 0;
  }
}

Token Parser::expect(TokKind k, const char* msg) {
  const Token& t = m_lex.peek();
  if (t.kind != k) throw ParseError{msg};
  m_lex.consume();
  return t;
}

std::unique_ptr<Expr> Parser::nud(Token t) {
  switch (t.kind) {
    case TokKind::Number:   return std::make_unique<LitNumber>(t.number);
    case TokKind::String:   return std::make_unique<LitString>(t.lexeme);
    case TokKind::Ident:    return std::make_unique<FieldRef>(t.lexeme);
    case TokKind::KW_NOT:   return std::make_unique<Not>(expression(90));
    case TokKind::LParen: {
      auto e = expression(0);
      expect(TokKind::RParen, "Missing ')'");
      return e;
    }
    case TokKind::Minus: { // unary minus: 0 - expr
      auto rhs = expression(60);
      return std::make_unique<Arith>(std::make_unique<LitNumber>(0.0), ArithOp::Sub, std::move(rhs));
    }
    default:
      throw ParseError{"Unexpected token"};
  }
}

std::unique_ptr<Expr> Parser::led(std::unique_ptr<Expr> left, Token op) {
  switch (op.kind) {
    case TokKind::Star:
    case TokKind::Slash:
    case TokKind::Plus:
    case TokKind::Minus: {
      int bp = (op.kind==TokKind::Star || op.kind==TokKind::Slash) ? 60 : 50;
      auto right = expression(bp);
      ArithOp a = ArithOp::Add;
      if (op.kind==TokKind::Minus) a = ArithOp::Sub;
      else if (op.kind==TokKind::Star) a = ArithOp::Mul;
      else if (op.kind==TokKind::Slash) a = ArithOp::Div;
      return std::make_unique<Arith>(std::move(left), a, std::move(right));
    }
    case TokKind::Eq:
    case TokKind::EqEq:
    case TokKind::Ne:
    case TokKind::Lt:
    case TokKind::Le:
    case TokKind::Gt:
    case TokKind::Ge: {
      CmpOp c = CmpOp::EQ;
      if (op.kind==TokKind::Ne) c = CmpOp::NE;
      else if (op.kind==TokKind::Lt) c = CmpOp::LT;
      else if (op.kind==TokKind::Le) c = CmpOp::LE;
      else if (op.kind==TokKind::Gt) c = CmpOp::GT;
      else if (op.kind==TokKind::Ge) c = CmpOp::GE;
      auto right = expression(41);
      return std::make_unique<Cmp>(std::move(left), c, std::move(right));
    }
    case TokKind::KW_AND: {
      auto right = expression(21);
      return std::make_unique<BoolBin>(std::move(left), BoolOp::AND, std::move(right));
    }
    case TokKind::KW_OR: {
      auto right = expression(11);
      return std::make_unique<BoolBin>(std::move(left), BoolOp::OR, std::move(right));
    }
    default:
      throw ParseError{"Unexpected infix operator"};
  }
}

std::unique_ptr<Expr> Parser::expression(int min_bp) {
  Token t = m_lex.peek();
  m_lex.consume();
  auto left = nud(t);
  while (true) {
    const Token& op = m_lex.peek();
    int bp = lbp(op);
    if (bp <= min_bp) break;
    m_lex.consume();
    left = led(std::move(left), op);
  }
  return left;
}

std::unique_ptr<Expr> Parser::parse_expr() { return expression(0); }




