// src/cli/order_hooks.cpp
// Real (but safe) implementations that reuse existing CLI command handlers.
// We avoid touching internals by calling cmd_REFRESH and cmd_TOP.

#include "order_hooks.hpp"
#include <sstream>

namespace xbase { class DbArea; }

// Forward-declare existing command handlers (defined elsewhere)
void cmd_REFRESH(xbase::DbArea& db, std::istringstream& iss);
void cmd_TOP    (xbase::DbArea& db, std::istringstream& iss);

void order_notify_mutation(xbase::DbArea& db) noexcept {
    try {
        std::istringstream none;
        cmd_REFRESH(db, none);  // politely ask the system to resync views/index state
    } catch (...) {
        // best-effort only
    }
}

void order_auto_top(xbase::DbArea& db) noexcept {
    try {
        std::istringstream none;
        cmd_TOP(db, none);      // move to first record after creating an index
    } catch (...) {
        // best-effort only
    }
}
