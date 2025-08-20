// cmd_order.cpp — consolidated ASCEND and DESCEND commands
// Safe to build even if indexing isn't wired yet.

#include "xbase.hpp"
#include "textio.hpp"

#include <sstream>
#include <iostream>
#include <string>

#include "order_state.hpp"
#include "xbase.hpp"
#include <sstream>
#include <iostream>

// If you still have stub versions in this file, hide them when indexing is ON
#if !DOTTALK_WITH_INDEX
void cmd_ASCEND(xbase::DbArea&, std::istringstream&) {
    std::cout << "ASCEND: acknowledged (no index support in this build).\n";
}
void cmd_DESCEND(xbase::DbArea&, std::istringstream&) {
    std::cout << "DESCEND: acknowledged (no index support in this build).\n";
}
#endif


using xbase::DbArea;

// Internal helper: prints standard "no table" message and returns true if handled
static inline bool ensure_table_open(DbArea& A) {
    if (!A.isOpen()) {
        std::cout << "No table open.\n";
        return true;
    }
    return false;
}
