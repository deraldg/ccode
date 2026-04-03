// src/cli/order_banner.hpp
// DotTalk++ — AREA banner formatter — header-only
//
// Wire-up: build an AreaFacts struct from your DbArea + order_state and
// print the returned strings in AREA (or anywhere you show state).

#pragma once
#include <string>
#include "order_path_resolver.hpp" // for IndexKind and via_phrase()

namespace dottalk { namespace order {

struct AreaFacts {
    // Required
    std::string file_rel;     // e.g., "dbf/students.dbf"
    uint32_t    recs {0};
    uint32_t    recno {0};

    // Order info
    bool        has_order {false};
    std::string tag;          // e.g., "LNAME" for CNX; empty for pure INX-ASC
    bool        asc {true};   // ASC/DESC flag

    // Container info (if any)
    IndexKind   kind { IndexKind::NONE };
    std::string index_rel;    // pretty path relative to data_root (or empty)
};

struct AreaBanner {
    std::string line1;  // "Current area: N"
    std::string line2;  // "  File: <file>  Recs: <n>  Recno: <n>"
    std::string line3;  // "  Order: <...>" (or "NATURAL")
    std::string line4;  // "  Active tag  : <name| (none)>" (omitted for NATURAL)
};

inline std::string order_phrase(const AreaFacts& f){
    if (!f.has_order || f.kind==IndexKind::NONE)
        return "NATURAL";
    // CNX with tag name
    if (f.kind==IndexKind::CNX) {
        const std::string dir = f.asc ? "ASC" : "DESC";
        return f.tag + " (" + dir + ") " + via_phrase(f.kind, f.index_rel);
    }
    // INX/IDX (legacy) — show direction and optional tag
    const std::string dir = f.asc ? "ASCEND" : "DESCEND";
    if (!f.tag.empty()) {
        return dir + " " + via_phrase(f.kind, f.index_rel) + "  Tag: " + f.tag;
    }
    return dir + " " + via_phrase(f.kind, f.index_rel);
}

inline AreaBanner format_banner(int area_number, const AreaFacts& f){
    AreaBanner b;
    b.line1 = "Current area: " + std::to_string(area_number);
    b.line2 = "  File: " + f.file_rel + "  Recs: " + std::to_string(f.recs) + "  Recno: " + std::to_string(f.recno);
    const auto ord = order_phrase(f);
    b.line3 = "  Order: " + ord;
    if (ord=="NATURAL") {
        b.line4.clear(); // omit for natural
    } else {
        b.line4 = "  Active tag  : " + (f.tag.empty() ? std::string("(none)") : f.tag);
    }
    return b;
}

}} // namespace dottalk::order
