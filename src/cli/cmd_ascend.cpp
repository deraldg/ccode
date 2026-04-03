#include "xbase.hpp"
#include "cli/order_state.hpp"
#include <iostream>


void cmd_ASCEND(xbase::DbArea& A, std::istringstream&) {
    if (!orderstate::hasOrder(A)) { std::cout << "No active index.\n"; return; }
    orderstate::setAscending(A, true);
    std::cout << "Order: ASCENDING.\n";
}


/*#include "xbase.hpp"
#include "order_state.hpp"
#include <iostream>

using namespace xbase;

void cmd_ASCEND(DbArea& area, std::istringstream& iss) {
    if (!order_state::hasOrder(area)) {
        std::cout << "No active order." << std::endl;
        return;
    }
    order_state::setAscending(area, true);
    std::cout << "Order set to ascending." << std::endl;
}

*/


