#pragma once
#include "xbase.hpp"
#include <string>

namespace predx {

// Evaluate a CLI expression, e.g. `FOR LAST_NAME $ "MIT"` or `GPA >= 3.2`.
// Type-aware (C/N/D/L). Supports UPPER()/LOWER() on the left side:
//   COUNT FOR UPPER(LAST_NAME) $ "MIT"
//   COUNT FOR LOWER(LAST_NAME) = "smith"
bool eval_expr(xbase::DbArea& A, std::string expr);

// Convenience if you already split into parts.
bool eval_triplet(xbase::DbArea& A,
                  std::string field, std::string op, std::string rhs);

// Global toggle for string comparisons.
// OFF (default): case-insensitive string compare and substring.
// ON:            case-sensitive string compare and substring.
void set_case_sensitive(bool on);
bool get_case_sensitive();

} // namespace predx
