#include <sstream>
#include <cstdio>

#include "xbase.hpp"
#include "xindex/attach.hpp"

void cmd_ASCEND(xbase::DbArea& area, std::istringstream&) {
    auto& mgr = xindex::ensure_manager(area);
    if (!mgr.has_active()) { std::puts("No active index."); return; }
    mgr.set_direction(true);
    std::puts("Order: ASCENDING.");
}
void cmd_DESCEND(xbase::DbArea& area, std::istringstream&) {
    auto& mgr = xindex::ensure_manager(area);
    if (!mgr.has_active()) { std::puts("No active index."); return; }
    mgr.set_direction(false);
    std::puts("Order: DESCENDING.");
}
