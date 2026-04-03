// src/cli/area_banner.hpp
// DotTalk++ — AREA banner formatter (declaration)

#pragma once
#include <string>
#include <cstdint>


// Must pull in IndexKind and via_phrase()
#include "order_path_resolver.hpp"  // defines dottalk::order::IndexKind and helpers

namespace dottalk { namespace order {

// Facts the banner needs (populate from DbArea + order_state)
struct AreaFacts {
    // Required
    std::string file_rel;     // e.g., "dbf/students.dbf"
    uint32_t    recs {0};
    uint32_t    recno {0};

    // Order info
    bool        has_order {false};
    std::string tag;          // "LNAME" if CNX; optional for INX/IDX
    bool        asc {true};   // ASC/DESC

    // Container info (if any)
    dottalk::order::IndexKind kind { dottalk::order::IndexKind::NONE };
    std::string index_rel;    // pretty path relative to data_root (or empty)
};

struct AreaBanner {
    std::string line1;  // "Current area: N"
    std::string line2;  // "  File: <file>  Recs: <n>  Recno: <n>"
    std::string line3;  // "  Order: <...>" or "  Order: NATURAL"
    std::string line4;  // "  Active tag  : ..." (empty for NATURAL)
};

// Build the user-facing order phrase (e.g., "LNAME (ASC) via CNX [indexes/students.cnx]")
std::string order_phrase(const AreaFacts& f);

// Format all AREA lines in a stable way
AreaBanner format_banner(int area_number, const AreaFacts& f);

}} // namespace dottalk::order
