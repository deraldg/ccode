# from D:\code\ccode
cmake -S . -B .\build-tests -DCMAKE_BUILD_TYPE=Release
cmake --build .\build-tests --config Release --target dottalkpp_where_cache_tests
ctest --test-dir .\build-tests -C Release -R where_cache_tests --output-on-failure
