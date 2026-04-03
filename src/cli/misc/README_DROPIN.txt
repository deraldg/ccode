DotTalk++ Drop-In Overlay: FoxPro Echo Matrix (Phase 1)

This bundle intentionally does NOT modify shell.cpp yet.

Files included:
  - src/cli/output_router.hpp
  - src/cli/output_router.cpp
  - src/cli/cmd_set.cpp

Integration notes:
  1) Ensure output_router.cpp is compiled/linked (CMake glob usually picks it up).
  2) cmd_set.cpp now supports:
       SET CONSOLE ON|OFF
       SET PRINT ON|OFF
       SET PRINT TO <file> | SET PRINT TO
       SET ALTERNATE ON|OFF
       SET ALTERNATE TO <file> | SET ALTERNATE TO
       SET TALK ON|OFF
     plus existing SET routes (INDEX/CNX/ORDER/FILTER/DELETED/etc.).

Next phase:
  - Refactor shell.cpp to route shell output and TALK echo via OutputRouter.
  - Then build a "learning list" to teach apps (LIST/DISPLAY/etc.) to print through the router.
