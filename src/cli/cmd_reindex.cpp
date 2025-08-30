// src/cli/cmd_reindex.cpp
#include <sstream>
#include <iostream>
#include "xbase.hpp"

// order hooks: refresh + optional cursor nudge
// (order_hooks.hpp in your tree doesn’t declare refresh, so forward-declare)
void order_notify_refresh(xbase::DbArea&);   // defined in order_hooks.cpp
void order_auto_top(xbase::DbArea&) noexcept;

void cmd_REINDEX(xbase::DbArea& a, std::istringstream&) {
    if (!a.isOpen()) { std::cout << "No table open.\n"; return; }
    // Rebuild all in-memory indexes using identity remap
    order_notify_refresh(a);   // calls through to xindex::IndexManager::on_pack(...)
    order_auto_top(a);         // optional: consistent UX after rebuild
    std::cout << "Reindexed: " << a.name() << "\n";
}
