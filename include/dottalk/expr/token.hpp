#pragma once
#include <string>

namespace dottalk { namespace expr {

enum class TokKind {
  End, Ident, Number, String,
  Eq, EqEq, Ne, Lt, Le, Gt, Ge,
  LParen, RParen,
  KW_NOT, KW_AND, KW_OR,
  Plus, Minus, Star, Slash        // <-- arithmetic
};

struct Token {
  TokKind kind{TokKind::End};
  std::string lexeme;
  double number{0.0};
};

}} // namespace
