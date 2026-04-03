// cmd_order.cpp ? consolidated ASCEND and DESCEND commands
// Safe to build even if indexing isn't wired yet.

#include "xbase.hpp"
#include "textio.hpp"

#include <sstream>
#include <iostream>
#include <string>

#include "cli/order_state.hpp"
#include "xbase.hpp"
#include <sstream>
#include <iostream>

// cmd_order.cpp  (top of file)
void cmd_ASCEND(xbase::DbArea&, std::istringstream&);
void cmd_DESCEND(xbase::DbArea&, std::istringstream&);



using xbase::DbArea;

// Internal helper: prints standard "no table" message and returns true if handled
static inline bool ensure_table_open(DbArea& A) {
    if (!A.isOpen()) {
        std::cout << "No table open.\n";
        return true;
    }
    return false;
}



