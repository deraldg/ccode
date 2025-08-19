#include "xindex/index_manager.hpp"
#include "xbase.hpp"
#include "textio.hpp"
#include <sstream>
#include <string>

void cmd_ASCEND(xbase::DbArea& area, std::istringstream&) {
    if (!area.is_open()) { textio::print("No table open.\n"); return; }
    auto* mgr = area.idx(); if (!mgr || !mgr->has_active()) { textio::print("No active index.\n"); return; }
    mgr->set_direction(true);
    textio::print("Order: ASCENDING.\n");
}
void cmd_DESCEND(xbase::DbArea& area, std::istringstream&) {
    if (!area.is_open()) { textio::print("No table open.\n"); return; }
    auto* mgr = area.idx(); if (!mgr || !mgr->has_active()) { textio::print("No active index.\n"); return; }
    mgr->set_direction(false);
    textio::print("Order: DESCENDING.\n");
}
