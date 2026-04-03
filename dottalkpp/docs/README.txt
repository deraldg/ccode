DotTalk++ DTS Smoke Tests

How to run each test (write a log):
  TEST <scriptfile> <logfile> VERBOSE

Examples:
  TEST struct_inx_active.dts struct_inx_active.log VERBOSE
  TEST struct_cnx_tags.dts   struct_cnx_tags.log   VERBOSE
  TEST list_order_flip.dts   list_order_flip.log   VERBOSE
  TEST full_baseline.dts     full_baseline.log     VERBOSE

Create a baseline (first good run):
  if not exist ..\baseline mkdir ..\baseline
  copy *.log ..\baseline\

Compare future runs vs baseline (Windows):
  fc /n struct_inx_active.log ..\baseline\struct_inx_active.log
  fc /n struct_cnx_tags.log   ..\baseline\struct_cnx_tags.log
  fc /n list_order_flip.log   ..\baseline\list_order_flip.log
  fc /n full_baseline.log     ..\baseline\full_baseline.log

Notes:
  - Scripts are read-only (they only list/inspect).
  - If your environment attaches STUDENTS.inx automatically on USE students,
    struct_inx_active.dts will show Active tag : lname.
  - For CNX tests we explicitly use: USE students noindex + SETCNX students.cnx.
  - These are snapshot tests. Diff logs to detect regressions.
