# src/memo/CMakeLists.txt
cmake_minimum_required(VERSION 3.20)

file(GLOB_RECURSE MEMO_SOURCES CONFIGURE_DEPENDS
  "${CMAKE_CURRENT_SOURCE_DIR}/*.c"
  "${CMAKE_CURRENT_SOURCE_DIR}/*.cc"
  "${CMAKE_CURRENT_SOURCE_DIR}/*.cxx"
  "${CMAKE_CURRENT_SOURCE_DIR}/*.cpp"
)

add_library(memo STATIC ${MEMO_SOURCES})

target_include_directories(memo
  PUBLIC
    ${CMAKE_SOURCE_DIR}/include
)

if (MSVC)
  target_compile_options(memo PRIVATE /W4 /permissive-)
else()
  target_compile_options(memo PRIVATE -Wall -Wextra -Wpedantic)
endif()
