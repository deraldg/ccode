Cherry Pack v2 (hardened) for DotTalk++

Includes:
  • src/cli/order_hooks.hpp         — safe no-op hooks: order_notify_mutation(), order_auto_top()
  • src/cli/cmd_append.cpp          — hardened APPEND with validation; calls order_notify_mutation(db)
  • tools/apply_index_autotop.ps1   — injects #include and order_auto_top(db) after 'Index written:' line
  • tools/apply_help_tweak.ps1      — updates HELP text for APPEND and removes APPEND_BLANK

How to apply:
1) Back up your repo (bundle.ps1, as usual).
2) Copy `src\cli\order_hooks.hpp` and `src\cli\cmd_append.cpp` into the same paths in your repo.
3) Run:
     powershell -ExecutionPolicy Bypass -File .\tools\apply_index_autotop.ps1
     powershell -ExecutionPolicy Bypass -File .\tools\apply_help_tweak.ps1
4) Rebuild & smoke:
     .\tools\rebuild_ccode.ps1
     .\tools\smoke_ccode.ps1

What you get:
- APPEND now rejects garbage tokens and non-numeric counts cleanly:
    • 'APPEND'            -> ok (1 blank)
    • 'APPEND BLANK'      -> ok
    • 'APPEND -B 5'       -> ok
    • 'APPEND frog'       -> Usage (no crash)
    • 'APPEND -B X'       -> Usage (no crash)
- Safe no-op index hooks now in place:
    • order_notify_mutation(db) after appends (ready for future index refresh)
    • order_auto_top(db) after INDEX ON (injected; no-op unless you wire it later)

These changes are intentionally low-risk and compile even if your index/order layer doesn’t expose real hooks yet.
