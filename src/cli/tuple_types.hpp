// src/cli/tuple_types.hpp
#pragma once

#include <string>
#include <vector>
#include <cstdint>

namespace dottalk {

// Where a piece of a tuple came from.
// Keep this simple; expand later (memo blocks, json row ids, sqlite rowid, etc.)
enum class TupleSourceKind : uint8_t {
    Unknown = 0,
    DBF     = 1,
    Memo    = 2,
    Derived = 3,
    JSON    = 4,
    SQLite  = 5
};

struct TupleColumn {
    std::string name;       // column label (resolved to canonical field name when possible)
    int         area_slot;  // work area slot (0..MAX_AREA-1) that owns it; -1 if unknown
    std::string field;      // field name in that area (or original token if unresolved)
};

struct TupleFragment {
    int            area_slot  = -1;
    int            recno      = 0;               // 1-based recno if known; 0 if unknown
    TupleSourceKind kind      = TupleSourceKind::Unknown;
    std::string    note;                         // optional breadcrumb
};

struct TupleRow {
    std::vector<TupleColumn>   columns;   // stable ordering
    std::vector<std::string>   values;    // aligned with columns
    std::vector<TupleFragment> fragments; // provenance

    bool empty() const { return columns.empty(); }
};

struct TupleBuildOptions {
    bool want_header              = false;  // (builder returns data row; header built by caller)
    bool header_area_prefix       = false;  // header names as AREA.FIELD
    bool values_area_prefix_echo  = false;  // for pretty printers
    bool strict_fields            = false;  // error on missing explicit field
    bool refresh_relations        = true;   // refresh SET RELATION before build
    std::string null_token        = "";     // pretty placeholder
};

} // namespace dottalk
