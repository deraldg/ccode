#pragma once
// Auto-open/close memo sidecars per work area.
// Call memo_auto_on_use() from USE after the table is open,
// and memo_auto_on_close() from CLOSE / before replacing a table in the same area.

#include <string>
#include <cstdint>

#include "memo_backend.hpp"
#include "xbase.hpp"

namespace cli_memo {

struct MemoConfig {
    bool autocreate = true;  // create sidecar if table has memo fields and file absent
    bool strict     = false; // if true and sidecar missing/corrupt => fail USE
};

// Global config setters/getters
void set_memo_config(const MemoConfig& cfg);
MemoConfig get_memo_config();

// Attach a memo backend to this area if the opened table schema has memo fields.
// - openedPath: the exact path used to open the DBF
// - hasMemoFields: true if schema has any 'M' fields
// Returns true on success; false and sets err on failure.
// If hasMemoFields is false, this detaches any prior sidecar and returns success.
bool memo_auto_on_use(xbase::DbArea& area,
                      const std::string& openedPath,
                      bool hasMemoFields,
                      std::string& err);

// Detach (flush+close) the memo backend bound to this area, if any.
void memo_auto_on_close(xbase::DbArea& area);

// Non-owning access to the bound backend, or nullptr if none.
dottalk::memo::IMemoBackend* memo_backend_for(xbase::DbArea& area);

// Backward-compat alias during transition.
inline dottalk::memo::IMemoBackend* memo_store_for(xbase::DbArea& area) {
    return memo_backend_for(area);
}

} // namespace cli_memo