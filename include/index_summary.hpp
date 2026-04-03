#pragma once
// src/cli/index_summary.hpp
// CLI-facing summary of the active index/order for a DbArea.

#include <string>
#include <vector>

namespace xbase { class DbArea; }

namespace dottalk {

struct IndexTagSummary {
    std::string tagName;   // Tag id or expression name
    std::string fieldName; // Primary field (best-effort)
    std::string type;      // "Character", "Numeric", etc. (best-effort)
    int         len{0};
    int         dec{0};
    bool        asc{true};
};

struct IndexSummary {
    enum class OrderKind { Physical, Ascending, Descending };

    // Standardized fields
    OrderKind                       kind{OrderKind::Physical};
    std::string                     order_str{"PHYSICAL"};   // "PHYSICAL" | "ASCEND" | "DESCEND"
    std::string                     index_path;              // e.g. "CUSTOMER.INX" | "ORDERS.CNX"
    std::string                     active_tag;              // CNX tag name (if any)
    std::vector<IndexTagSummary>    tags;                    // INX: one tag; CNX: multiple

    // Legacy compatibility (for older wsreport/status code)
    bool        hasOrder{false};
    std::string container;            // mirrors index_path
    bool        isCnx{false};
    std::string tag;                  // mirrors active_tag
    bool        ascending{true};      // true => ASCEND
    long        recCount{0};          // best-effort (left 0 if unknown)
};

// Implemented in index_summary.cpp
IndexSummary summarize_index_for(xbase::DbArea& area);

// Convenience alias for older call sites
inline IndexSummary summarize_index(xbase::DbArea& area) {
    return summarize_index_for(area);
}

} // namespace dottalk

// Legacy unqualified exports (CLI code often relies on these)
using dottalk::IndexSummary;
using dottalk::IndexTagSummary;
using dottalk::summarize_index;



