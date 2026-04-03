#pragma once

#include <string>
#include <optional>

namespace xbase {
    class DbArea;
}

namespace expr {
    struct Node;   // placeholder for your AST node type
}

namespace dt::predicate {

// ---------------------------
// Value & type system
// ---------------------------

enum class ValueType {
    Null,
    Character,
    Numeric,
    Date,
    Logical
};

struct Value {
    ValueType    type{ValueType::Null};
    std::string  s;   // character or normalized representation
    double       n{}; // numeric or date-serial if you choose that later
    bool         b{}; // logical

    // Convenience queries (no logic yet; implemented in .cpp)
    bool is_null()      const noexcept { return type == ValueType::Null; }
    bool is_char()      const noexcept { return type == ValueType::Character; }
    bool is_numeric()   const noexcept { return type == ValueType::Numeric; }
    bool is_date()      const noexcept { return type == ValueType::Date; }
    bool is_logical()   const noexcept { return type == ValueType::Logical; }
};

// ---------------------------
// Field metadata
// ---------------------------

struct FieldTypeInfo {
    char ftype{'C'};   // 'C','N','D','L' etc.
    int  length{0};
    int  decimals{0};
    int  index{0};     // 1-based DbArea field index
    bool valid{false};
};

// ---------------------------
// Record context
// ---------------------------

struct RecordContext {
    xbase::DbArea* area{nullptr};
    int            recno{0};      // 1-based record number; 0 = unknown/unset
};

// ---------------------------
// Core evaluation API
// ---------------------------

// Lookup a field by name in the current DbArea.
// Returns a fully-populated FieldTypeInfo or valid=false if not found.
FieldTypeInfo lookup_field(const RecordContext& rc,
                           const std::string&   field_name);

// Read and normalize a single field value from DBF into a typed Value.
// This is where FieldTypeInfo + value_normalize.hpp come together.
Value get_field_value_normalized(const RecordContext& rc,
                                 const FieldTypeInfo& fti,
                                 std::string*         error_out = nullptr);

// Evaluate an expression tree against a record context, returning a typed Value.
// The expression tree type (expr::Node) is intentionally opaque here.
Value eval_expr(const expr::Node& expr,
                const RecordContext& rc,
                std::string*         error_out = nullptr);

// Evaluate an expression as a logical predicate on a specific record.
// Returns true/false on success; on error, returns false and fills error_out
// (callers may choose to treat errors as non-match or raise them to the user).
bool record_matches(xbase::DbArea&  area,
                    int             recno,
                    const expr::Node* pred,
                    std::string*    error_out = nullptr);

} // namespace dt::predicate



