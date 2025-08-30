# Integration Guide: Memo Sidecar (.dtx)

This guide shows where to connect the new memo store into DotTalk++ without breaking existing features.

## 1) Field Type
- Add `M` (Memo) to your field type enum and schema loader.
- For each memo field in a record, allocate **8 bytes** in-row to store `u64 object_id` (0 = NULL).
- Optionally store a cached `u32 length` next to the id (12 bytes total) if your row format has room.

## 2) Open/Create Table
- On `USE <table>`, detect presence of memo fields.
- If memo fields exist:
  - If `<table>.dtx` exists, `MemoStore::open(basepath)`
  - Else: if opening read-only or strict mode, ERROR; if CREATE, call `MemoStore::create(basepath)`.

## 3) Record Lifecycle
- APPEND: memo fields default to id=0.
- REPLACE <memo> WITH "text": `put()` -> id; write back id.
- REPLACE <memo> WITH FILE "p": read file bytes; `put()` -> id.
- DELETE: leave id as-is; memo reclaimed on PACK.
- RECALL: leave id unchanged.
- PACK: copy live memos to new sidecar and remap IDs; swap atomically.

## 4) CLI/DLI Wiring
- Implement verbs:
  - TYPE: resolve field -> id -> `get()` -> print/paginate or `TO` file.
  - SET MEMO WIDTH/EDITOR/TRUNCATE: update runtime settings.
  - MEMOCHECK/MEMOSTATS: forward to MemoStore and render output.
- LIST/DISPLAY:
  - When printing memo, if id==0 print empty.
  - Else fetch length quickly (via `stat()`), and fetch body **only** if needed.
  - Respect `SET MEMO WIDTH` in LIST; DISPLAY may stream.

## 5) Indexing
- Disallow memo fields in index expressions at parse/compile time.
- Ensure order state code paths do not attempt to read full memo bodies.

## 6) Export/Import
- CSV export: write memo text directly; escape quotes and newlines.
- Option `--memo=files` (optional): write to external files and store paths in CSV.

## 7) Admin
- MEMOCHECK [REPAIR]: surface stats, CRC errors; with REPAIR, rebuild id-map.
- MEMOSTATS: show Used/Free/Objects/NextId (+ fragmentation estimates if tracked).

## 8) Errors & Recovery
- Missing `.dtx` with memo fields => fail open with guidance.
- On bad CRC, allow `--ignore-crc` flag to force reads (at user risk).
- `repair()` path rebuilds id-map from object scan.
