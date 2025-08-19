#pragma once
#include "xindex/index_manager.hpp" // brings in forward decl for xbase::DbArea via index_manager.hpp

namespace xindex {
IndexManager& ensure_manager(xbase::DbArea& area);
} // namespace xindex
