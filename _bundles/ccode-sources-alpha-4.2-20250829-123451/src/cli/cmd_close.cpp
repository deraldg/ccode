// src/cli/cmd_close.cpp
#include <sstream>
#include <iostream>
#include "xbase.hpp"
#include "order_state.hpp"   // for orderstate::clearOrder

// NOTE: must be in the global namespace, not static, exact signature:
void cmd_CLOSE(xbase::DbArea& a, std::istringstream& iss)
{
    (void)iss; // unused
    try {
        orderstate::clearOrder(a);  // ok if there's no active order
    } catch (...) {
        // ignore; we're closing the table anyway
    }
    a.close();
    std::cout << "Closed.\n";
}
