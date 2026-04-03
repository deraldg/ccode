// src/cli/order_notify_stub.cpp
// Link shim for mutation notifier. Now wires to order_nav cache invalidation.

#include "xbase.hpp"
#include "cli/order_nav.hpp"

using namespace xbase;

void order_notify_mutation(DbArea& a) {
    order_nav_invalidate(a);
}



