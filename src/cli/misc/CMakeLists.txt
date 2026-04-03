# src/cli/browse/CMakeLists.txt — builds the refactored BROWSE module
cmake_minimum_required(VERSION 3.20)

add_library(dottalk_browse STATIC
  browse_cmd.cpp
  browse_args.cpp
  browse_format.cpp
  browse_filters.cpp
  browse_order.cpp
  browse_session.cpp
  browse_edit.cpp
  browse_io.cpp
  browse_util.cpp
)

target_include_directories(dottalk_browse
  PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/.. # so consumers can #include "browse/browse_cmd.hpp"
  PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}
)

# Consumers (dottalkpp) already include project-wide include directories.
# Link to internal libs as needed once you migrate the real implementations.
target_link_libraries(dottalk_browse
  PUBLIC
    # xbase, xindex, memo, expr, etc., are linked at the top-level target.
)

# Enable warnings for this subtarget (MSVC-friendly)
if (MSVC)
  target_compile_options(dottalk_browse PRIVATE /W4)
else()
  target_compile_options(dottalk_browse PRIVATE -Wall -Wextra -Wpedantic)
endif()
