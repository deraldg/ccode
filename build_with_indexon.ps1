# From repo root
cd C:\Users\deral\code\ccode

# 1) Clean the build
if (Test-Path build) { Remove-Item -Recurse -Force build }

# 2) Configure with indexing enabled
cmake -S . -B build -G "Visual Studio 17 2022" -A x64 -D DOTTALK_WITH_INDEX=ON

# 3) Rebuild the executable
cmake --build build --config Release --target dottalkpp -- /t:Rebuild
