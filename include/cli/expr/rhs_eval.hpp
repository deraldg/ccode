#pragma once

#include <string>

namespace xbase { class DbArea; }

namespace dottalk::expr {

struct EvalValue;

// Canonical RHS evaluator entry point.
// areaOrNull == nullptr => scalar/no-table mode
// areaOrNull != nullptr => field-aware mode
EvalValue eval_rhs(xbase::DbArea* areaOrNull,
                   const std::string& exprText,
                   std::string* errOut = nullptr);

} // namespace dottalk::expr
