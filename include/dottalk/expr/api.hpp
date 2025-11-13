#pragma once
#include <memory>
#include <string>
#include "dottalk/expr/ast.hpp"

namespace dottalk { namespace expr {

struct CompileResult {
  std::unique_ptr<Expr> program;
  std::string error;
  explicit operator bool() const { return program!=nullptr && error.empty(); }
};

CompileResult compile_where(const std::string& text);

}} // namespace dottalk::expr

// ---- Back-compat inline forwarder in global namespace ----
inline dottalk::expr::CompileResult compile_where(const std::string& text) {
  return dottalk::expr::compile_where(text);
}
