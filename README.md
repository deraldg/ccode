# xbase — Stage 3 (C++ OOP)

Modernized, RAII-friendly C++ port of the ANSI C xBase CLI. Focus is on
clean types, safe IO, and a tiny command shell. Indexing remains a stub.

## Build (Make)
```bash
make
./dottalkpp
```

## Build (CMake)
```bash
cmake -S . -B build
cmake --build build --config Release
./build/dottalkpp
```

## Commands
Matches the Stage‑2 CLI subset:
- `USE <file>` open a DBF (adds `.dbf` if missing)
- `LIST [FIELD ...]` dump records from current to end
- `FIELD <FIELD ...>` print those fields for current record
- `DISPLAY` print all fields of current record
- `TOP | BOTTOM | GOTO <n> | SKIP <delta>` navigate
- `EDIT <FIELD> <STRING>` edit in-memory
- `SAVE` persist current record
- `APPEND` append blank record
- `DELETE` mark deleted
- `COUNT | RECNO`
- `SELECT <AREA>` 0..9
- `HELP | QUIT`

## Notes
- DBF parsing uses the classic dBASE III header/field layout.
- Field data is treated as raw strings; numeric/date types are not parsed yet.
- Index functions intentionally omitted here; a pluggable `Index` interface can be added later.

## Next ideas
- Strongly-typed fields with converters.
- Real .NDX/.IDX reader/writer via a strategy interface.
- Tests (googletest) around record navigation and field edits.
