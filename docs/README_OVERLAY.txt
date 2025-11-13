
DotTalk++ — Full Source Overlay (link-fix minimal set)
======================================================

This overlay provides the smallest set of files to fix the current link errors:
  • Duplicate cmd_* symbols from cli_overloads.cpp
  • Missing dispatch_command referenced by reader.cpp
  • Missing xindex::db_* free-function wrappers referenced by db_adapter.cpp

Files included
--------------
1) src/cli/cli_overloads.cpp         (REPLACE your file with this one)
   - Keeps only current-area get/set helpers.
   - Removes all cmd_*(DbArea&, ...) overloads that duplicated real implementations.

2) src/cli/dispatch_shim.cpp         (NEW file)
   - Defines dispatch_command(const std::string&) and forwards to shell_dispatch_line().

3) src/xindex/cli_wrappers.cpp       (NEW file)
   - Implements xindex::db_* functions used by db_adapter.cpp,
     delegating to DbArea/IndexManager.

How to apply
------------
1) Copy the 'src' folder from this overlay on top of your repo root:
       C:\Users\deral\code\ccode\
   Resulting paths:
       C:\Users\deral\code\ccode\src\cli\cli_overloads.cpp
       C:\Users\deral\code\ccode\src\cli\dispatch_shim.cpp
       C:\Users\deral\code\ccode\src\xindex\cli_wrappers.cpp

2) Clean + configure + build (Release shown, Debug is fine too):
       pwsh -NoProfile -ExecutionPolicy Bypass -Command "
         cd C:\Users\deral\code\ccode;
         if (Test-Path .\build) { Remove-Item -Recurse -Force .\build };
         cmake -S . -B build -G 'Visual Studio 17 2022' -A x64;
         cmake --build build --config Release -- /m
       "

Notes
-----
• Your CMake uses file globs, so the two NEW .cpp files will be picked up automatically.
• No edits to existing .hpp headers are required.
• The tiny EOF tracker in cli_wrappers.cpp emulates legacy NEXT/EOF behavior;
  feel free to wire into IndexManager later for richer index semantics.
• The shim does nothing if no current area is set; it only exists to satisfy reader.cpp.
• If you later decide the reader path is obsolete, you can remove reader.cpp and the shim.

Timestamp: 2025-09-10 02:44:35
