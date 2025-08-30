cmake_minimum_required(VERSION 3.16)
project(dottalkpp VERSION 0.3.0 LANGUAGES CXX)

# ---- Options ---------------------------------------------------------------
option(DOTTALK_WITH_INDEX "Build CLI with indexing features and verbs" ON)

# ---- Toolchain / Defaults --------------------------------------------------
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

if(MSVC)
  add_compile_options(/W4 /permissive- /MP /utf-8)
else()
  add_compile_options(-Wall -Wextra -Wpedantic)
endif()

# ---- Paths -----------------------------------------------------------------
set(CCODE_ROOT "${CMAKE_SOURCE_DIR}")
set(CCODE_INC  "${CCODE_ROOT}/include")
set(CCODE_SRC  "${CCODE_ROOT}/src")
set(CCODE_XI   "${CCODE_SRC}/xindex")
set(CCODE_CLI  "${CCODE_SRC}/cli")
set(CCODE_XB   "${CCODE_SRC}/xbase")

# ---- xindex library --------------------------------------------------------
file(GLOB XI_SOURCES "${CCODE_XI}/*.cpp")
# Exclude legacy implementation that targets the old DbArea API
list(FILTER XI_SOURCES EXCLUDE REGEX ".*/simple_index\\.cpp$")

add_library(xindex STATIC ${XI_SOURCES})
target_include_directories(xindex PUBLIC "${CCODE_INC}")

# ---- xbase library ---------------------------------------------------------
# Prefer files under src/xbase; if none, take non-CLI/non-xindex sources under src/
file(GLOB_RECURSE XBASE_SOURCES "${CCODE_XB}/*.cpp")
if(NOT XBASE_SOURCES)
  file(GLOB XBASE_FALLBACK "${CCODE_SRC}/*.cpp")
  if(XBASE_FALLBACK)
    list(FILTER XBASE_FALLBACK EXCLUDE REGEX "/(cli|xindex)/")
    set(XBASE_SOURCES "${XBASE_FALLBACK}")
  endif()
endif()

if(NOT XBASE_SOURCES)
  message(FATAL_ERROR "No xbase sources found under ${CCODE_XB} or fallback in ${CCODE_SRC}.")
endif()

# Explicitly exclude dbarea_introspection.cpp (was experimenting with new API)
list(FILTER XBASE_SOURCES EXCLUDE REGEX ".*/dbarea_introspection\\.cpp$")

add_library(xbase STATIC ${XBASE_SOURCES})
target_include_directories(xbase PUBLIC "${CCODE_INC}")

# NOTE: avoid circular deps; link both libs only at the executable
# (no target_link_libraries(xbase xindex) and no target_link_libraries(xindex xbase))

# ---- CLI executable --------------------------------------------------------
file(GLOB CLI_SOURCES "${CCODE_CLI}/*.cpp")
if(NOT CLI_SOURCES)
  message(FATAL_ERROR "No CLI sources found in ${CCODE_CLI}")
endif()

# Belt-and-suspenders: exclude any accidental file named src_cli_cmd_status.cpp
list(FILTER CLI_SOURCES EXCLUDE REGEX ".*/src_cli_cmd_status\\.cpp$")

add_executable(dottalkpp ${CLI_SOURCES})
target_include_directories(dottalkpp PRIVATE "${CCODE_INC}")
target_link_libraries(dottalkpp PRIVATE xbase xindex)

# ---- DotTalk++ Expr sources ----
target_include_directories(dottalkpp PRIVATE ${CMAKE_SOURCE_DIR}/include)

set(DOTTALK_EXPR_SOURCES
  src/cli/expr/lexer.cpp
  src/cli/expr/parser.cpp
  src/cli/expr/eval.cpp
  src/cli/expr/api.cpp
  src/cli/expr/for_parser.cpp
)
target_sources(dottalkpp PRIVATE ${DOTTALK_EXPR_SOURCES})


# Make the flag visible to the code for #if DOTTALK_WITH_INDEX blocks
if(DOTTALK_WITH_INDEX)
  target_compile_definitions(dottalkpp PRIVATE DOTTALK_WITH_INDEX=1)
else()
  target_compile_definitions(dottalkpp PRIVATE DOTTALK_WITH_INDEX=0)
endif()

target_compile_definitions(dottalkpp PRIVATE
    XINDEX_ADAPT_HAS_RECLEN
    XINDEX_ADAPT_HAS_FILENAME)

# ---- Status banner ---------------------------------------------------------
message(STATUS "================ CMake Config ================")
message(STATUS "Source dir : ${CCODE_ROOT}")
message(STATUS "Build dir  : ${CMAKE_BINARY_DIR}")
message(STATUS "CLI dir    : ${CCODE_CLI}")
message(STATUS "Indexing   : ${DOTTALK_WITH_INDEX}")
message(STATUS "xindex src : ${CCODE_XI}")
message(STATUS "xbase  src : ${CCODE_XB}")
message(STATUS "=============================================")
