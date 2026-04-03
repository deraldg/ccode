
// cli_bridge.cpp  (updated)
#include "cli/order_state.hpp"
#include "cli/cli_bridge.hpp"

namespace xindex_cli {

bool db_index_attached(const xbase::DbArea& A) {
    return orderstate::hasOrder(A);
}

std::string db_index_path(const xbase::DbArea& A) {
    return orderstate::orderName(A);
}

std::string db_active_cnx_tag(const xbase::DbArea& A) {
    if (!orderstate::isCnx(A)) return std::string();
    return orderstate::activeTag(A);
}

bool db_order_asc(const xbase::DbArea& A) {
    return orderstate::isAscending(A);
}

} // namespace xindex_cli

namespace xindex_cli_internal {
void set_active(const xbase::DbArea& /*A*/, const std::string& /*key*/) {
    // no-op for now
}
} // namespace xindex_cli_internal



