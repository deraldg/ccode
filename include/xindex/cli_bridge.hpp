// include/xindex/cli_bridge.hpp
#pragma once

#include <string>

namespace xbase { class DbArea; }

namespace xindex_cli {

// Returns true if a logical order (index) is currently active for A.
bool db_index_attached(const xbase::DbArea& A);

// User-facing name/path for the active index (container or single index).
// e.g., "STUDENTS.inx", "lname.inx". Empty if none.
std::string db_index_path(const xbase::DbArea& A);

// Alias of db_index_path kept for clarity where callers say “active index path”.
std::string db_active_index_path(const xbase::DbArea& A);

} // namespace xindex_cli

// Internal hooks used by CLI glue. Keep separate namespace to avoid public API drift.
namespace xindex_cli_internal {

// Mark an index/tag as “active” for CLI/xindex coordination.
// Current implementation is a no-op shim; replace when deeper sync is needed.
void set_active(const xbase::DbArea& A, const std::string& key);

} // namespace xindex_cli_internal
