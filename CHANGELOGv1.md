# DotTalk++ Changelog

## Alpha v3 (2025-08-19)
### Highlights
- **Indexing Framework Integrated**
  - INDEX, SET INDEX/SET INDEX TO, ASCEND, DESCEND, SEEK now wired through index manager.
  - Sidecar `.inx` file support added (load/save).
- **Core Commands Restored**
  - USE, LIST, COUNT, DISPLAY, TOP, BOTTOM, FIELDS, DELETE, RECALL, PACK fully operational.
  - APPEND and APPEND BLANK stabilized (numeric + interactive modes).
- **Shell Improvements**
  - Command registry refreshed, with aliases and optional niceties (e.g., CLS, SET INDEX TO).
  - External shell escape (`!`) functional.
- **UI/Output**
  - Color themes integrated (GREEN/AMBER/DEFAULT).
  - Output alignment improvements for LIST and FIELDS.

### Known Issues
- APPEND BLANK still enters interactive mode if no numeric argument.
- LOCATE, REPLACE, STATUS, STRUCT, ZAP not yet available.
- SEEK exact-match semantics need additional verification.

### Next Steps
- Wire index lifecycle to APPEND/DELETE/RECALL/PACK hooks.
- Complete RECNO, STRUCT, STATUS, REPLACE, LOCATE implementations.
- Add paging for LIST output.
- Stabilize HELP topics (banner vs. extended help separation).

---
