#include "xbase.hpp"
#include "order_state.hpp"
#include <iostream>
#include "order_state.hpp"

void cmd_ASCEND(xbase::DbArea& A, std::istringstream&) {
    if (!orderstate::hasOrder(A)) { std::cout << "No active index.\n"; return; }
    orderstate::setAscending(A, true);
    std::cout << "Order: ASCENDING.\n";
}
