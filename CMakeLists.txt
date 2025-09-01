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
set(CCODE_MEMO "${CCODE_SRC}/memo")

# =========================
#  Libraries (xindex/xbase)
# =========================

# ---- xindex library --------------------------------------------------------
file(GLOB XI_SOURCES "${CCODE_XI}/*.cpp")
# Exclude legacy implementation that targets the old DbArea API
list(FILTER XI_SOURCES EXCLUDE REGEX ".*/simple_index\\.cpp$")

add_library(xindex STATIC ${XI_SOURCES})
target_include_directories(xindex PUBLIC "${CCODE_INC}")

# ---- xbase library ---------------------------------------------------------
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

# Explicitly exclude experimental file if present
list(FILTER XBASE_SOURCES EXCLUDE REGEX ".*/dbarea_introspection\\.cpp$")

add_library(xbase STATIC ${XBASE_SOURCES})
target_include_directories(xbase PUBLIC "${CCODE_INC}")

# ==========================================================
#  Memo sidecar library (.dtx)
# ==========================================================
file(GLOB MEMO_SOURCES "${CCODE_MEMO}/*.cpp")
if(MEMO_SOURCES)
  add_library(memo STATIC ${MEMO_SOURCES})
  target_include_directories(memo PUBLIC "${CCODE_INC}")
  set(MEMO_ENABLED TRUE)
else()
  message(STATUS "No memo sources found under ${CCODE_MEMO} (memo sidecar disabled).")
  set(MEMO_ENABLED FALSE)
endif()

# ===================
#  Executable (CLI)
# ===================
file(GLOB CLI_SOURCES "${CCODE_CLI}/*.cpp")
if(NOT CLI_SOURCES)
  message(FATAL_ERROR "No CLI sources found in ${CCODE_CLI}")
endif()

# Belt-and-suspenders: exclude any accidental file named src_cli_cmd_status.cpp
list(FILTER CLI_SOURCES EXCLUDE REGEX ".*/src_cli_cmd_status\\.cpp$")

add_executable(dottalkpp ${CLI_SOURCES})

# Make headers available to the CLI:
#  - ${CCODE_INC} for public headers (include/…)
#  - ${CCODE_SRC} so includes like "cli/replace_multi.hpp" resolve
target_include_directories(dottalkpp PRIVATE
  "${CCODE_INC}"
  "${CCODE_SRC}"
)

# === Browsetui V2 stubs (safe pilot) ========================================
# Link the new stub sources so cmd_browse.cpp resolves its references.
target_sources(dottalkpp PRIVATE
  ${CMAKE_SOURCE_DIR}/src/ui/layout_manager.cpp
  ${CMAKE_SOURCE_DIR}/src/ui/renderer_v2_stub.cpp
  ${CMAKE_SOURCE_DIR}/src/input/legacy_input_adapter.cpp
  ${CMAKE_SOURCE_DIR}/src/data/record_gateway_dbf_stub.cpp
  ${CMAKE_SOURCE_DIR}/src/browse/controller.cpp
)

# Ensure the new public headers are visible
target_include_directories(dottalkpp PRIVATE
  "${CMAKE_SOURCE_DIR}/include"
)

# Silence MSVC getenv() warning in browse_flags.hpp
if (MSVC)
  target_compile_definitions(dottalkpp PRIVATE _CRT_SECURE_NO_WARNINGS)
endif()
# ============================================================================


# Ensure utf8_init.cpp is compiled into the exe (fixes LNK2019 on dottalk::init_utf8)
target_sources(dottalkpp PRIVATE
  ${CCODE_CLI}/utf8_init.cpp
)

# Link only at the executable level to avoid circular deps. Include memo if present.
if(MEMO_ENABLED)
  target_link_libraries(dottalkpp PRIVATE xbase xindex memo)
else()
  target_link_libraries(dottalkpp PRIVATE xbase xindex)
endif()

# ---- DotTalk++ Expr sources (keep as-is) -----------------------------------
set(DOTTALK_EXPR_SOURCES
  src/cli/expr/lexer.cpp
  src/cli/expr/parser.cpp
  src/cli/expr/eval.cpp
  src/cli/expr/api.cpp
  src/cli/expr/for_parser.cpp
)
target_sources(dottalkpp PRIVATE ${DOTTALK_EXPR_SOURCES})

# ---- Compile-time feature flags --------------------------------------------
if(DOTTALK_WITH_INDEX)
  target_compile_definitions(dottalkpp PRIVATE DOTTALK_WITH_INDEX=1)
else()
  target_compile_definitions(dottalkpp PRIVATE DOTTALK_WITH_INDEX=0)
endif()

target_compile_definitions(dottalkpp PRIVATE
  XINDEX_ADAPT_HAS_RECLEN
  XINDEX_ADAPT_HAS_FILENAME
)

# ---- Status banner ---------------------------------------------------------
message(STATUS "================ CMake Config ================")
message(STATUS "Source dir : ${CCODE_ROOT}")
message(STATUS "Build dir  : ${CMAKE_BINARY_DIR}")
message(STATUS "CLI dir    : ${CCODE_CLI}")
message(STATUS "Indexing   : ${DOTTALK_WITH_INDEX}")
message(STATUS "xindex src : ${CCODE_XI}")
message(STATUS "xbase  src : ${CCODE_XB}")
message(STATUS "Memo src   : ${CCODE_MEMO}")
message(STATUS "Memo sidecar enabled: ${MEMO_ENABLED}")
message(STATUS "=============================================")
