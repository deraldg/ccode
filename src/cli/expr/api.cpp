#include "dottalk/expr/api.hpp"
#include "dottalk/expr/parser.hpp"

namespace dottalk { namespace expr {

CompileResult compile_where(const std::string& text) {
  try {
    Parser p(text);
    auto e = p.parse_expr();
    return CompileResult{ std::move(e), "" };
  } catch (const ParseError& pe) {
    return CompileResult{ nullptr, pe.message };
  } catch (...) {
    return CompileResult{ nullptr, "Unknown parse error" };
  }
}

}} // namespace dottalk::expr
