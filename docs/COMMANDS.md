# DotTalk++ Command Reference
_Generated: 2025-08-15_

This lists the CLI verbs, baseline syntax, and notes. An asterisk (*) indicates planned/not yet implemented.

> **General**  
> - Commands are case-insensitive.  
> - Paging uses `cli::Settings.page_lines`.  
> - Current area/cursor is managed by `xbase::DbArea`.

---

## Core / Session

### `HELP`
Show command list (with `*` for unavailable).

### `AREA`
Show the currently selected area index (or manage areas if multi-area is enabled).

### `SELECT <n>`
Select area number `<n>` (if multi-area build).

### `QUIT` / `EXIT`
Leave the program.

---

## File / Table

### `USE <stem>`
Open `<stem>.dbf` into the selected area.  
**Example:** `USE students`

### `DISPLAY`
Echo currently opened file and status (records, current recno, deleted flag).  
_(Exact output may vary; implemented in `cmd_display.cpp`.)_

### `STATUS` * (planned)
Detailed engine status.

### `STRUCT` * (planned)
Describe file structure.

### `CREATE` * (planned)
Create a new table.

---

## Navigation

### `TOP`
Go to first record and show context.

### `BOTTOM`
Go to last record and show context.

### `GOTO <n>`
Go to record number `<n>` (1-based).

### `SKIP <delta>` * (planned)
Move relative by `<delta>` records.

---

## Browse / Output

### `FIELDS`
List schema (index, name, type, length, decimals).  
No arguments.

### `LIST [<count>]`
List records in a fixed-width grid with a header row derived from `FIELDS`.
- `count` (optional): number of rows to display; default = `Settings.page_lines`.
- Uses fixed widths computed from `FieldDef.len` (or sensible minimums).
- Character fields are space-padded to their defined width; numerics are right-aligned; dates show in `YYYYMMDD`; logical as `T/F`.

**Examples**
```
LIST
LIST 40
```

### `COUNT`
Print the total number of (optionally filtered) records.  
_Currently baseline: total count; predicate support TBD._

### `COLOR <GREEN|AMBER|DEFAULT>`
Set UI color theme for headings and hrules.

### `EXPORT <csvPath>`
Export current table to CSV (all columns).

### `IMPORT <csvPath>`
Append rows from CSV into the current table, mapping by header names.

### `COPY <destStem>`
Copy current file to a new DBF `<destStem>.dbf`.  
_Implementation may copy structure and all rows._

---

## Edit / Maintenance

### `APPEND`
Append a blank record (fields default/blank).

### `DELETE [<n>]`
Mark current (or record `<n>`) as deleted.

### `RECALL [<n>]`  (alias: `UNDELETE`)
Un-delete current (or record `<n>`).

### `PACK`
Permanently remove deleted records.

### `REPLACE ...` * (planned)
In-place update expressions.

---

## Search / Index

### `FIND <expr>` / `LOCATE <expr>` *
Locate records matching a predicate.  
_Current build: `FIND` available (string contains / numeric compares as supported by `predicates.hpp`); `LOCATE` planned._

### `SEEK <key>`
Position by index key (when index is active).

### `INDEX ...` * (planned)
Index management (create/open/set order/etc.).

---

## Version / About

### `VERSION`
Show program version/build info.

---

## Aliases
- `UNDELETE` → `RECALL`

---

## Notes
- All verbs are registered through `cli::CommandRegistry` in `shell.cpp`; built-ins are intercepted before registry dispatch.
- As of this build, `LIST` and `FIELDS` implement wide headers, fixed-width columns, and proper padding/justification.
- Paging defaults to `Settings.page_lines` (20). Increase it with: `SET PAGE <n>` * (planned).
