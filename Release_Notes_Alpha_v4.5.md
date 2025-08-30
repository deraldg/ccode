# DotTalk++ — Alpha v4.5 Release Notes
**Date:** 2025-08-25

## Overview
Alpha v4.5 delivers the first working indexing pipeline for DotTalk++. You can now build index files (`.inx`) from a DBF, attach them to an area, and list records in indexed order.

## What’s New
- **INDEX command (writer):** `INDEX ON <field|#n> TAG <name>` generates `<name>.inx` in the new **1INX** binary format.
- **SETINDEX (attach):** Activates an index file for the current area: `SETINDEX <tag-or-path>`.
- **LIST (reader):** Traverses in index order when an index is active (falls back to physical order otherwise).
- **Field matching hardened:** Name canonicalization (trim/pad-strip, uppercase, `[A-Z0-9_]` only). Numeric fallback via `#n` (1-based field index).
- **Order state:** `orderstate` tracks active index name/path and direction (ASC/DESC).

## Commands Affected
- `INDEX ON <field|#n> TAG <name>`  
  - Example: `INDEX ON LAST_NAME TAG N4`  
  - Example: `INDEX ON #3 TAG N3` (indexes the 3rd field)
- `SETINDEX <tag|path>`  
  - Example: `SETINDEX N4`
- `LIST [ALL|N] [FOR <fld> <op> <val>]`  
  - Respects active index. Uses physical order if none.
- `ASCEND` / `DESCEND`  
  - Toggle current order direction.
- `FIELDS`  
  - Use to confirm field positions (`#n`).

## 1INX File Format (v1)
- **Header**
  - Magic: `1INX` (4 bytes)
  - `u16` version = 1
  - `u16` nameLen
  - `nameLen` bytes: original expression/token (e.g., `LAST_NAME`, `#3`)
  - `u32` count (number of entries)
- **Entries (repeated `count` times)**
  - `u16` keyLen
  - `keyLen` bytes: key (as stored from the field value)
  - `u32` recno (1-based record number)

> Reader: `cmd_LIST` uses `xindex::SimpleIndex` to load 1INX and iterate entries in order.

## Known Behaviors / Quirks
- **Blank-key rows** bubble to the top in ascending order. You can `LIST ALL` to see them explicitly or later adopt a policy to skip/group them.
- **Case sensitivity:** Field matching is **case-insensitive** and tolerant to padding/garbage, but stored keys are not normalized beyond the original field data.
- **Legacy indexes:** Old `.inx` files (not in 1INX v1 format) will be reported as “unrecognized index format”. Rebuild with `INDEX`.

## Compatibility
- DBF I/O unchanged (no breaking changes to `xbase`).  
- Indexes are an additive feature and can be ignored (physical order still works).

## Suggested Workflow
```text
USE students
FIELDS                           # confirm column numbers
INDEX ON #3 TAG N_LAST           # or: INDEX ON LAST_NAME TAG N_LAST
SETINDEX N_LAST
LIST
DESCEND                          # optional order toggle
LIST 10
```

## Validation Checklist
- **Build index by name:** `INDEX ON LAST_NAME TAG N4` → `Index written: N4.inx`  
- **Attach index:** `SETINDEX N4` → `Index set: N4.inx`  
- **List respects order:** `LIST` shows rows sorted by last name.  
- **Fallback works:** `SETINDEX OFF` (or clear order) → `LIST` uses physical order.  
- **Numeric fallback:** `INDEX ON #3 TAG N3` works identically.

## Next Steps (Planned for v4.6+)
1. **Index lifecycle & maintenance**
   - Auto-attach/close with DBF open/close.
   - Update indexes on `APPEND`, `DELETE`, `RECALL`, `REPLACE`, `ZAP`, `PACK`.
2. **Multi-index and selection**
   - `SET INDEX TO tag1, tag2` (stack), `SET ORDER` for quick switch.
3. **Metadata & collation**
   - Store collation/expr in header; surface via `STATUS` or `DISPLAY STATUS`.
4. **Robustness**
   - Auto-detect/repair stale or corrupt indexes.
5. **UI polish**
   - Optional suppression/grouping of blank-key rows unless `ALL`.

---

**Snapshot:** This document and the diagram accompany the archive: `ccode-alpha.4.5-YYYYMMDD.zip`.
