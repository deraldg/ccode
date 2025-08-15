cmake_minimum_required(VERSION 3.16)
project(dottalkpp VERSION 0.3.0 LANGUAGES CXX)


# ---- Toolchain / Defaults ----
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

if(MSVC)
  add_compile_options(/W4 /permissive- /MP)
else()
  add_compile_options(-Wall -Wextra -Wpedantic)
endif()

# ---- Paths ----
set(CCODE_ROOT "${CMAKE_SOURCE_DIR}")
set(CCODE_INC  "${CCODE_ROOT}/include")
set(CCODE_SRC  "${CCODE_ROOT}/src")
set(CCODE_XI   "${CCODE_SRC}/xindex")
set(CCODE_CLI  "${CCODE_SRC}/cli")

# ---- xindex core -----------------------------------------------------------
file(GLOB XI_SOURCES "${CCODE_XI}/*.cpp")
if(NOT XI_SOURCES)
  message(FATAL_ERROR "No xindex sources found in ${CCODE_XI}")
endif()

add_library(xindex STATIC ${XI_SOURCES})
target_include_directories(xindex PUBLIC "${CCODE_INC}")

# ---- xbase core ------------------------------------------------------------
# Prefer src/xbase/*.cpp. If that folder doesn't exist, fall back to top-level src/*.cpp
# but EXCLUDE cli/ and xindex/ files.
file(GLOB_RECURSE XBASE_SRC_FROM_DIR "${CCODE_SRC}/xbase/*.cpp")
set(XBASE_SOURCES "${XBASE_SRC_FROM_DIR}")

if(NOT XBASE_SOURCES)
  file(GLOB XBASE_SRC_FALLBACK "${CCODE_SRC}/*.cpp")
  # Remove any files that live under cli/ or xindex/
  if(XBASE_SRC_FALLBACK)
    list(FILTER XBASE_SRC_FALLBACK EXCLUDE REGEX "/(cli|xindex)/")
    set(XBASE_SOURCES "${XBASE_SRC_FALLBACK}")
  endif()
endif()

if(NOT XBASE_SOURCES)
  message(FATAL_ERROR "No xbase sources found. Expected either ${CCODE_SRC}/xbase/*.cpp or non-cli/non-xindex files under ${CCODE_SRC}.")
endif()

add_library(xbase STATIC ${XBASE_SOURCES})
target_include_directories(xbase PUBLIC "${CCODE_INC}")
target_link_libraries(xbase PUBLIC xindex)

# ---- CLI -------------------------------------------------------------------
file(GLOB CLI_SOURCES "${CCODE_CLI}/*.cpp")
if(NOT CLI_SOURCES)
  message(FATAL_ERROR "No CLI sources found in ${CCODE_CLI}")
endif()

add_executable(dottalkpp ${CLI_SOURCES})
target_include_directories(dottalkpp PRIVATE "${CCODE_INC}")
target_link_libraries(dottalkpp PRIVATE xbase xindex)

# ---- Status banner ---------------------------------------------------------
message(STATUS "================================")
message(STATUS "  Source dir : ${CCODE_ROOT}")
message(STATUS "  Build dir  : ${CMAKE_BINARY_DIR}")
message(STATUS "  Layout     : INTEGRATED (no externals/)")
message(STATUS "  CLI dir    : ${CCODE_CLI}")
message(STATUS "================================")
