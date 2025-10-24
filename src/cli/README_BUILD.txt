Turbo Vision Demo — Build Instructions (Windows / VS 2022)

1) Open "Developer PowerShell for VS 2022".
2) From the repo root (where CMakeLists.txt is), run:
   cmake -S . -B build -G "Visual Studio 17 2022" -A x64
   cmake --build build --config Release
   .\build\Release\dottalk_tui.exe

MinGW alternative:
   cmake -S . -B build -G "MinGW Makefiles" -DCMAKE_BUILD_TYPE=Release
   cmake --build build -j
   .\build\dottalk_tui.exe
