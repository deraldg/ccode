#pragma once

#include <filesystem>
#include <string>

#include "common/path_state.hpp"

namespace dottalk::paths {

namespace fs = std::filesystem;

// Initialize all well-known slots from a DATA root.
void init_defaults(const fs::path& data_root);

// Human-readable dump for WSREPORT / diagnostics.
std::string dump();

// Convenience overload: set a slot from a path-like value.
void set_slot_from_value(Slot slot, const fs::path& value);

// Re-exported common state helpers expected by CLI callers.
using dottalk::paths::get_slot;
using dottalk::paths::reset;
using dottalk::paths::slot_from_string;
using dottalk::paths::state;

} // namespace dottalk::paths