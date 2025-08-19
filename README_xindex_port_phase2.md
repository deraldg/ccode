# DotTalk++ Indexing Port — Phase 2
Implements persistence of indexes to a sidecar `.inx` file (minimal JSON) and adds a runtime ASC/DESC toggle honored by LIST/TOP/BOTTOM.

## Files
- `include/xindex/` — `index_spec.hpp`, `index_key.hpp`, `index_tag.hpp`, `index_manager.hpp`
- `src/xindex/` — `index_tag.cpp`, `index_manager.cpp` (with save/load)
- `src/cli/` — `cmd_index.cpp`, `cmd_setindex.cpp`, `cmd_seek.cpp`, `cmd_order.cpp`

## CMake
```cmake
add_library(xindex
  src/xindex/index_tag.cpp
  src/xindex/index_manager.cpp
)
target_include_directories(xindex PUBLIC include)
target_link_libraries(xindex PRIVATE xbase)

target_link_libraries(dottalkpp PRIVATE xindex)
```

## DbArea hooks (new/assumed)
Add methods or confirm these exist:
```cpp
std::string current_dbf_path() const;           // path to current .dbf
int         record_count() const;
bool        is_deleted(int recno) const;
std::string get_field_as_string(int recno, const std::string& field) const;
double      get_field_as_double(int recno, const std::string& field) const;
void        goto_rec(int recno);
bool        is_open() const;
xindex::IndexManager* idx();
const xindex::IndexManager* idx() const;
```
On `USE`:
```cpp
idx_ = std::make_unique<xindex::IndexManager>(*this);
idx_->load_for_table(current_dbf_path());
```
On `CLOSE/QUIT`:
```cpp
if (idx_) idx_->save(current_dbf_path());
idx_.reset();
```

## Command registry
- `INDEX`    → `cmd_INDEX`
- `SET INDEX` or `SET ORDER` → `cmd_SETINDEX`
- `SEEK`     → `cmd_SEEK`
- `ASCEND`   → `cmd_ASCEND`
- `DESCEND`  → `cmd_DESCEND`

## LIST/TOP/BOTTOM integration
```cpp
if (auto* mgr = area.idx(); mgr && mgr->has_active()) {
    auto const& ents = mgr->active()->entries();
    if (mgr->direction_ascending()) {
        for (auto const& kr : ents) /* render rec kr.recno */ ;
    } else {
        for (auto it = ents.rbegin(); it != ents.rend(); ++it) /* render rec it->recno */ ;
    }
} else {
    // natural order
}
```

## Notes
- Deleted rows are **excluded** from tags.
- `on_replace` uses remove+reinsert; correct and simple.
- `on_pack` fully rebuilds (safe).
- Runtime `ASCEND/DESCEND` is independent of the tag spec; changing it does **not** rewrite the `.inx`.
