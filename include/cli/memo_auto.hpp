#pragma once
// Auto-open/close .dtx memo sidecars per work area.
// You call memo_auto_on_use() from your USE command after the table is open,
// and memo_auto_on_close() from CLOSE/when replacing a table in the same area.

#include <string>
#include <cstdint>
#include "memo/memostore.hpp"
#include "xbase.hpp"    // your DbArea

namespace cli_memo {

struct MemoConfig {
    bool autocreate = true; // create .dtx if table has memo fields and file absent
    bool strict     = false; // if true and .dtx missing/corrupt => fail USE
};

// Global config setters/getters (cheap singletons)
void set_memo_config(const MemoConfig& cfg);
MemoConfig get_memo_config();

// Attach a MemoStore to this area if the opened table schema has memo fields.
// - openedPath: the exact path the user passed to USE (with or without .dbf).
// - hasMemoFields: true if schema has any 'M' memo fields.
// Returns true on success; false and sets err on failure.
// NOTE: If hasMemoFields is false, this will detach any previous sidecar (no-op success).
bool memo_auto_on_use(xbase::DbArea& area,
                      const std::string& openedPath,
                      bool hasMemoFields,
                      std::string& err);

// Detach (flush+close) the MemoStore bound to this area (if any).
void memo_auto_on_close(xbase::DbArea& area);

// Non-owning access to the bound store (or nullptr if none).
xbase::memo::MemoStore* memo_store_for(xbase::DbArea& area);

} // namespace cli_memo
