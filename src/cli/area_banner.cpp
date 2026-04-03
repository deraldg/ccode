// src/cli/area_banner.cpp
// DotTalk++ — AREA banner formatter (definition)

#include "area_banner.hpp"
#include <utility>

namespace dottalk { namespace order {

std::string order_phrase(const AreaFacts& f){
    using dottalk::order::IndexKind;
    if (!f.has_order || f.kind == IndexKind::NONE) {
        return "NATURAL";
    }

    if (f.kind == IndexKind::CNX) {
        const std::string dir = f.asc ? "ASC" : "DESC";
        return f.tag + " (" + dir + ") " + via_phrase(f.kind, f.index_rel);
    }

    // INX or IDX
    const std::string dir = f.asc ? "ASCEND" : "DESCEND";
    if (!f.tag.empty()) {
        return dir + " " + via_phrase(f.kind, f.index_rel) + "  Tag: " + f.tag;
    }
    return dir + " " + via_phrase(f.kind, f.index_rel);
}

AreaBanner format_banner(int area_number, const AreaFacts& f){
    AreaBanner b;
    b.line1 = "Current area: " + std::to_string(area_number);
    b.line2 = "  File: " + f.file_rel + "  Recs: " + std::to_string(f.recs) + "  Recno: " + std::to_string(f.recno);

    const std::string ord = order_phrase(f);
    b.line3 = "  Order: " + ord;
    if (ord == "NATURAL") {
        b.line4.clear(); // omit line4 for NATURAL
    } else {
        b.line4 = "  Active tag  : " + (f.tag.empty() ? std::string("(none)") : f.tag);
    }
    return b;
}

}} // namespace dottalk::order
