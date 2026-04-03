#include "xindex/index_manager.hpp"   // must DEFINE xindex::IndexManager
#include "xbase.hpp"

namespace xbase {

// Out-of-line defaults: keeps IndexManager complete in this TU (MSVC-safe).
DbArea::DbArea(DbArea&&) = default;
DbArea& DbArea::operator=(DbArea&&) = default;

} // namespace xbase
