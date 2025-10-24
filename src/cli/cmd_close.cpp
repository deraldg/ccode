// src/cli/cmd_close.cpp
#include <sstream>
#include <iostream>
#include "xbase.hpp"
#include "order_state.hpp"   // for orderstate::clearOrder
#include "cli/memo_auto.hpp" // <-- memo sidecar binder

// NOTE: must be in the global namespace, not static, exact signature:
void cmd_CLOSE(xbase::DbArea& a, std::istringstream& iss)
{
    (void)iss; // unused
    // Detach memo sidecar bound to this work area (mirrors index close-on-close)
    cli_memo::memo_auto_on_close(a);

    try {
        orderstate::clearOrder(a);  // ok if there's no active order
    } catch (...) {
        // ignore; we're closing the table anyway
    }
    a.close();
    std::cout << "Closed.\n";
}
