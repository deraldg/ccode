# tests/CMakeLists.txt
add_executable(tv_probe tv_probe.cpp)
target_link_libraries(tv_probe PRIVATE tvision)

# Turbo Vision's tv.h requires these to materialize the types we touch.
target_compile_definitions(tv_probe PRIVATE Uses_TRect Uses_TEvent)

# Ensure CTest runs the right config-specific binary (Debug/Release)
add_test(NAME tv-probe COMMAND $<TARGET_FILE:tv_probe>)
