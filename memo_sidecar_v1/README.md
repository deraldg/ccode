# Memo Fields via Sidecar Pointers (.dtx) — Starter Pack

This package contains:
- `include/memo/dtx_format.hpp` — on-disk structures & constants
- `include/memo/memostore.hpp` — public API for the memo store
- `src/memo/memostore.cpp` — skeleton implementation (non-functional)
- `docs/CLI_Memo_Help.txt` — help text to merge into your CLI/DLI help system
- `docs/DotScript_Memo_Grammar.txt` — EBNF-style grammar additions
- `docs/Integration_Steps.md` — how/where to wire into your codebase

### Next Steps (suggested)
1. Wire `MemoStore::create/open` to real file I/O and header read/write (DTX v1).
2. Implement put/get/update/erase with CRC and simple free-list.
3. Integrate with record codec: memo fields store a `u64 object_id` (0 = NULL).
4. Add verbs: TYPE, SET MEMO WIDTH/EDITOR/TRUNCATE, MEMOCHECK, MEMOSTATS.
5. Add PACK logic to compact sidecar and remap ids atomically.
6. Build unit tests for MemoStore and integration tests for CLI/DLI verbs.

> This is aligned with our "Alpha 5.0 Shake-Down" plan. Do not merge into main
> until shake-down is green; develop in a feature branch (e.g., `feature/memo-sidecar-v1`).

