Additional DTS tests + CI runner
=================================

Added scripts (read-only):
  - inxi_attach_switching.dts
  - cnx_tag_keyword_form.dts
  - banner_consistency.dts

Runner:
  - run_tests.ps1  → executes all DTS scripts, writes logs to .\logs, and diffs against .\baseline

Usage:
  1) Place these files alongside the earlier smoke tests.
  2) First baseline:
       if not exist baseline mkdir baseline
       # run once to generate logs
       powershell -ExecutionPolicy Bypass -File .\run_tests.ps1 -ExePath .\build\src\Release\dottalkpp.exe
       copy .\logs\*.log .\baseline\
  3) Subsequent runs (CI or local):
       powershell -ExecutionPolicy Bypass -File .\run_tests.ps1 -ExePath .\build\src\Release\dottalkpp.exe
     - Script returns non-zero on any diff and points to the *.diff file.

Notes:
  - Tests are read-only; they do not mutate DBF data.
  - cnx_tag_keyword_form.dts locks the TAG keyword behavior to prevent regressions.
  - banner_consistency.dts ensures LIST banner always matches STATUS/STRUCT INDEX.
