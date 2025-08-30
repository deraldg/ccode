HELP Sync Pack v1

- Overwrites src\cli\cmd_help.cpp with a curated HELP that lists only implemented commands.
- Backs up your original cmd_help.cpp with a timestamp suffix.

How to apply:
1) Unzip into repo root.
2) Run:
   powershell -ExecutionPolicy Bypass -File .\tools\apply_help_sync.ps1
3) Rebuild:
   .\tools\rebuild_ccode.ps1
