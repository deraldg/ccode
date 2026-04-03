#include "xbase.hpp"
#include "cli/order_state.hpp"
#include <iostream>
void cmd_DESCEND(xbase::DbArea& A, std::istringstream&) {
    if (!orderstate::hasOrder(A)) { std::cout << "No active index.\n"; return; }
    orderstate::setAscending(A, false);
    std::cout << "Order: DESCENDING.\n";
}



