#pragma once
// Unique field registry (per work area).
// Phase 1: session-scoped in-memory flags; no engine changes required.

#include <string>
#include <unordered_map>
#include <unordered_set>
#include <vector>

namespace xbase { class DbArea; }

namespace unique_reg {

// Return a stable bucket name for the current area (Phase 1 uses "AREA").
// You can later change this to alias/area number without breaking callers.
std::string current_alias_or_area_name(xbase::DbArea& A);

// Register/unregister a field as unique for the current area.
void set_unique_field(xbase::DbArea& A, const std::string& field_name, bool on);

// Query whether a field is marked unique for the current area.
bool is_unique_field(xbase::DbArea& A, const std::string& field_name);

// List all unique fields for the current area.
std::vector<std::string> list_unique_fields(xbase::DbArea& A);

// For diagnostics (read-only snapshot of the registry).
const std::unordered_map<std::string, std::unordered_set<std::string>>& snapshot();

} // namespace unique_reg
