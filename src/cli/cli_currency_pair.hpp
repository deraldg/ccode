#pragma once

#include <string>

namespace xbase {
class DbArea;
}

namespace cli_currency {

// Returns true if field name ends with _CUR or _CU (case-insensitive).
bool is_currency_pair_field_name(const std::string& field_name);

// Returns base name for a paired currency field.
// Examples:
//   PRICE_CUR -> PRICE
//   PRICE_CU  -> PRICE
// If not a paired currency field, returns the original name unchanged.
std::string currency_pair_base_name(const std::string& field_name);

// Validate and normalize a paired currency-code field.
//
// Behavior:
// - If field1 is not a *_CUR or *_CU field, returns true and copies raw_value
//   to normalized_value.
// - If field1 is a paired currency field:
//   - paired base field must exist
//   - paired base field must be type Y
//   - raw_value must normalize to exactly 3 uppercase letters
//
// On success:
//   - returns true
//   - normalized_value contains canonical value to store
//
// On failure:
//   - returns false
//   - err contains user-facing reason
bool validate_and_normalize_currency_pair_field(
    xbase::DbArea& area,
    int field1,
    const std::string& raw_value,
    std::string& normalized_value,
    std::string& err);

} // namespace cli_currency