// src/xindex/cli_bridge.cpp
#include "xindex/cli_bridge.hpp"

// order_state.hpp lives under src/cli — include relatively so xindex target compiles
#include "../cli/order_state.hpp"

#include <string>

namespace xindex_cli {

bool db_index_attached(const xbase::DbArea& A) {
    return orderstate::hasOrder(A);
}

std::string db_index_path(const xbase::DbArea& A) {
    if (!orderstate::hasOrder(A)) return std::string();
    if (auto n = orderstate::orderName(A); !n.empty()) return n;
    return std::string("index");
}

std::string db_active_index_path(const xbase::DbArea& A) {
    return db_index_path(A);
}

} // namespace xindex_cli

namespace xindex_cli_internal {

void set_active(const xbase::DbArea& /*A*/, const std::string& /*key*/) {
    // No-op shim. Purpose: satisfy CLI’s expectation of an xindex coordination hook.
    // If/when the xindex layer needs explicit notification of the active tag/container,
    // implement it here (and keep the signature stable).
}

} // namespace xindex_cli_internal
