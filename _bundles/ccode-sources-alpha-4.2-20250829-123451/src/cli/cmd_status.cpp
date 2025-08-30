// src/cli/cmd_status.cpp — drop-in
// Prints workspace, table stats, and order/index info.

#include <iostream>
#include <sstream>
#include <string>
#include <vector>

#include "xbase.hpp"
#include "xindex/dbarea_adapt.hpp"   // db_record_count(), db_recno()
#include "xindex/index_manager.hpp"
#include "xindex/attach.hpp"         // ensure_manager(...)
#include "order_state.hpp"           // orderstate::{isAscending,hasOrder,orderName}

void cmd_STATUS(xbase::DbArea& area, std::istringstream& /*args*/)
{
    // ── Workspace ─────────────────────────────────────────────────────────────
    std::cout << "Workspace\n";
    std::cout << "  File: ";
#if defined(XINDEX_ADAPT_HAS_FILENAME)
    // If your adapter exposes filename(), show it.
    std::cout << xindex::db_filename(area) << "\n\n";
#else
    std::cout << "\n\n";
#endif

    // ── Table ─────────────────────────────────────────────────────────────────
    const int recs  = xindex::db_record_count(area);
    int recno_val   = -1;
#if defined(XINDEX_ADAPT_HAS_RECNO)
    recno_val = xindex::db_recno(area);
#endif

    std::cout << "Table\n";
    std::cout << "  Records: " << recs << "\n";
    std::cout << "  Recno:   " << (recno_val >= 0 ? std::to_string(recno_val) : std::string("(n/a)")) << "\n";
#if defined(XINDEX_ADAPT_HAS_RECLEN)
    std::cout << "  Bytes/rec: " << xindex::db_record_length(area) << "\n\n";
#else
    std::cout << "  Bytes/rec: " << "\n\n";
#endif

    // ── Order / Index ─────────────────────────────────────────────────────────
    std::cout << "Order / Index\n";
    const bool ascending = orderstate::isAscending(area);
    std::cout << "  Order direction: " << (ascending ? "ASCEND" : "DESCEND") << "\n";

    const std::string active = orderstate::hasOrder(area) ? orderstate::orderName(area)
                                                          : std::string("(none)");
    std::cout << "  Active tag: " << (active.empty() ? std::string("(none)") : active) << "\n";

    auto& mgr = xindex::ensure_manager(area);
    const std::vector<std::string> tags = mgr.listTags();

    std::cout << "  Index tags loaded: " << tags.size() << "\n";
    for (const std::string& tag : tags) {
        const bool isActive = (!active.empty() && active == tag);
        const std::string expr = mgr.exprFor(tag); // empty if manager doesn't know

        std::cout << "    " << (isActive ? "* " : "  ") << tag;
        if (!expr.empty()) {
            std::cout << "  -> " << expr;
        }
        std::cout << "\n";
    }
}
