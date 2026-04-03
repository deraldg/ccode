#pragma once
#include <string>

namespace xbase { class DbArea; }

namespace dottalk {

// Minimal shape consumed by area_banner.cpp
struct OrderInfo {
    bool        active = false;       // true if a non-NATURAL order is active
    std::string tag;                  // e.g., "LNAME"
    std::string direction;            // "ASC" or "DESC"
    std::string kind;                 // "CNX", "INX", or "IDX"
    std::string container_rel;        // path relative to data/ (e.g., "indexes/students.cnx")
};

// Stubbed resolver. Return NATURAL for now; you can later populate from your real order state.
OrderInfo query_active_order(const xbase::DbArea& area,
                             const std::string& data_dir_abs);

} // namespace dottalk
