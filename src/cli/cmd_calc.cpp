#include <iostream>
#include <sstream>
#include <optional>
#include "xbase.hpp"
#include "dottalk/expr/api.hpp"
#include "dottalk/expr/ast.hpp"
#include "dottalk/expr/eval.hpp"
#include "dottalk/expr/glue_xbase.hpp"


// CALC <field> = <expr>
// Currently evaluates using real data; TODO: write back to field once type mapping is confirmed.
void cmd_CALC(xbase::DbArea& area, std::istringstream& in) {
  std::string field;
  if (!(in >> field)) { std::cout << "Usage: CALC <field> = <expr>\n"; return; }
  char eq = 0;
  if (!(in >> eq) || eq != '=') { std::cout << "Usage: CALC <field> = <expr>\n"; return; }

  std::string expr_text;
  std::getline(in, expr_text);
  if (expr_text.empty()) { std::cout << "Usage: CALC <field> = <expr>\n"; return; }

  auto cr = dottalk::expr::compile_where(expr_text);
  if (!cr.program) { std::cout << "Parse error: " << cr.error << "\n"; return; }

  auto rv = dottalk::expr::glue::make_record_view(area);

  // For now, just evaluate and echo the result (no assignment yet).
  if (auto ar = dynamic_cast<dottalk::expr::Arith*>(cr.program.get())) {
    std::cout << "VALUE: " << ar->evalNumber(rv) << "\n";
    return;
  }
  if (auto ln = dynamic_cast<dottalk::expr::LitNumber*>(cr.program.get())) {
    std::cout << "VALUE: " << ln->v << "\n";
    return;
  }
  if (auto ls = dynamic_cast<dottalk::expr::LitString*>(cr.program.get())) {
    std::cout << "VALUE: " << ls->v << "\n";
    return;
  }
  std::cout << "VALUE: " << (cr.program->eval(rv) ? "1" : "0") << "\n";
}
