# ---- LMDB backend smoke test -----------------------------------------------

add_executable(dottalkpp_lmdb_backend_smoke
  ${CMAKE_CURRENT_SOURCE_DIR}/test_lmdb_backend.cpp
)

target_link_libraries(dottalkpp_lmdb_backend_smoke
  PRIVATE
    xindex
)

target_include_directories(dottalkpp_lmdb_backend_smoke
  PRIVATE
    ${CMAKE_SOURCE_DIR}/include
)

# Optional: make it show up in `ctest`
# enable_testing() is typically called at top-level; if already enabled, this is fine.
add_test(NAME dottalkpp_lmdb_backend_smoke COMMAND dottalkpp_lmdb_backend_smoke)