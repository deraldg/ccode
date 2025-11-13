cmake_minimum_required(VERSION 3.20)
project(expr_tests LANGUAGES CXX)

# Compute repo root (…/ccode) from tests/expr/
get_filename_component(PROJ_ROOT "${CMAKE_CURRENT_LIST_DIR}/../.." ABSOLUTE)

# Prefer the overlay location; fall back to your legacy location
set(EXPR_SRC_DIR "${PROJ_ROOT}/src/dottalk/expr")
if (NOT EXISTS "${EXPR_SRC_DIR}/api.cpp")
  set(EXPR_SRC_DIR "${PROJ_ROOT}/src/cli/expr")
endif()

add_executable(expr_tests
  test_expr.cpp
)

target_compile_features(expr_tests PRIVATE cxx_std_17)

target_include_directories(expr_tests PRIVATE
  ${PROJ_ROOT}/include
)

# Link just the expr engine sources
target_sources(expr_tests PRIVATE
  ${EXPR_SRC_DIR}/api.cpp
  ${EXPR_SRC_DIR}/eval.cpp
  ${EXPR_SRC_DIR}/for_parser.cpp
  ${EXPR_SRC_DIR}/lexer.cpp
  ${EXPR_SRC_DIR}/parser.cpp
)

if (MSVC)
  target_compile_options(expr_tests PRIVATE /W3 /permissive-)
endif()
