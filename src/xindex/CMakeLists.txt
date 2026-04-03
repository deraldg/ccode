# src/xindex/CMakeLists.txt
cmake_minimum_required(VERSION 3.20)

file(GLOB_RECURSE XINDEX_SOURCES CONFIGURE_DEPENDS
  "${CMAKE_CURRENT_SOURCE_DIR}/*.c"
  "${CMAKE_CURRENT_SOURCE_DIR}/*.cc"
  "${CMAKE_CURRENT_SOURCE_DIR}/*.cxx"
  "${CMAKE_CURRENT_SOURCE_DIR}/*.cpp"
)

add_library(xindex STATIC ${XINDEX_SOURCES})

target_include_directories(xindex
  PUBLIC
    ${CMAKE_SOURCE_DIR}/include
)

target_compile_definitions(xindex
  PUBLIC DOTTALK_WITH_INDEX=$<BOOL:${DOTTALK_WITH_INDEX}>
)

if (MSVC)
  target_compile_options(xindex PRIVATE /W4 /permissive-)
else()
  target_compile_options(xindex PRIVATE -Wall -Wextra -Wpedantic)
endif()

# ---- LMDB (vcpkg) -----------------------------------------------------------
# vcpkg prints:
#   find_package(unofficial-lmdb CONFIG REQUIRED)
#   target_link_libraries(main PRIVATE unofficial::lmdb::lmdb)
#
# We link it to xindex so lmdb.h is on the include path when compiling lmdb_backend.cpp.
if (DOTTALK_WITH_INDEX)
  find_package(unofficial-lmdb CONFIG REQUIRED)
  target_link_libraries(xindex PRIVATE unofficial::lmdb::lmdb)
endif()