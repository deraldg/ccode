#pragma once

#include <string>
#include <optional>

namespace dt::predicate {

struct Value;
struct FieldTypeInfo;
struct RecordContext;

// ---------------------------
// Literal & coercion helpers
// ---------------------------

// Construct canonical Values for literals.
Value make_null();
Value make_character(const std::string& s);
Value make_numeric(double n);
Value make_date_from_yyyymmdd(const std::string& yyyymmdd);
Value make_logical(bool b);

// Coerce an existing Value to logical.
// Returns true on success and writes to out; false on failure (error_out optional).
bool coerce_to_logical(const Value& v,
                       bool&        out,
                       std::string* error_out = nullptr);

// Coerce/canonicalize a literal string into a Value as if it were
// being entered for a specific field type.
//
// Example usage (later, inside parser/eval):
//   auto v = normalize_literal_for_field('D', 8, 0, "11/05/2025");
//
Value normalize_literal_for_field(char              ftype,
                                  int               flen,
                                  int               fdec,
                                  const std::string& raw_literal,
                                  std::string*      error_out = nullptr);

// Convert a normalized-for-compare string (from util::normalize_for_compare)
// into a Value with the right type tag.
Value value_from_normalized(char              ftype,
                            int               flen,
                            int               fdec,
                            const std::string& normalized,
                            std::string*      error_out = nullptr);

// Read, normalize, and convert a DBF field into Value in one shot.
// Thin wrapper around get_field_value_normalized, provided here so callers
// that only know about RecordContext/FieldTypeInfo can stay in this header.
Value field_value_from_dbf(const RecordContext& rc,
                           const FieldTypeInfo& fti,
                           std::string*         error_out = nullptr);

} // namespace dt::predicate



