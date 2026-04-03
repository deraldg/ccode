rmdir /s /q build
cmake -S D:/code/ccode -B D:/code/ccode/build -DCMAKE_BUILD_TYPE=Release -DCMAKE_TOOLCHAIN_FILE=%VCPKG_ROOT%\scripts\buildsystems\vcpkg.cmake
cmake --build build --config Release --target dottalkpp
