# CHANGELOG

This log summarizes the three-stage migration from an old ANSI C codebase to a modern C++ design.

## Stage 1 — Cleaned C (types/prototypes/pointers)
- **Safer indices**: Replaced `char` indices with `int` (`areanum`, `ord`, `select_rec.a`) to avoid negative/overflowed array indexing.
- **Prototype hygiene**: Added missing prototypes (e.g., `close_area`), removed implicit declarations; unified headers.
- **Pointer arithmetic**: `charproc()` now receives `areanum` instead of using the global `area`; avoids accidental cross-area reads.
- **Static buffer helpers**: `dbname()` / `dxname()` clarified as scratch helpers (documented non-reentrancy).
- **Index stubs**: Index layer compiles; functions return safe sentinels (`EOIX`). No unexpected crashes.
- **Build**: Simple portable Makefile (C99).

## Stage 2 — Usable C CLI
- **CLI parity**: `USE`, `LIST`, `FIELD`, `DISPLAY`, `TOP/BOTTOM/GOTO/SKIP`, `EDIT`, `SAVE`, `APPEND`, `DELETE`, `COUNT`, `RECNO`, `SELECT`, `HELP` working.
- **Index operations**: Left as safe no-ops (by design for Stage 2).
- **Error handling**: Added basic checks after IO ops; consistent messages.
- **Docs**: README with build and usage.

## Stage 3 — C++ OOP (RAII, std::string/vector)
- **DbArea** class: Encapsulates file handle, header, fields, record buffer, and current record state; no globals.
- **XBaseEngine**: Owns an array of `DbArea` (work areas) and current-area selection.
- **Record model**: `get(int)`, `set(int, std::string)`, `readCurrent()`, `writeCurrent()`, `appendBlank()`, `deleteCurrent()`; navigation via `top/bottom/gotoRec/skip`.
- **CLI (dottalkpp)**: Thin shell compatible with Stage 2 commands.
- **Index strategy**: Introduced a pluggable `IndexStrategy` interface (in the *indexed* variant) to support real or proprietary B-tree implementations.
- **Builds**: Makefile + CMake.
