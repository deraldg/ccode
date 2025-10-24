# src/xbase/CMakeLists.txt
cmake_minimum_required(VERSION 3.20)

file(GLOB_RECURSE XBASE_SOURCES CONFIGURE_DEPENDS
  "${CMAKE_CURRENT_SOURCE_DIR}/*.c"
  "${CMAKE_CURRENT_SOURCE_DIR}/*.cc"
  "${CMAKE_CURRENT_SOURCE_DIR}/*.cxx"
  "${CMAKE_CURRENT_SOURCE_DIR}/*.cpp"
)

add_library(xbase STATIC ${XBASE_SOURCES})

target_include_directories(xbase
  PUBLIC
    ${CMAKE_SOURCE_DIR}/include
)

target_compile_definitions(xbase
  PUBLIC DOTTALK_WITH_INDEX=$<BOOL:${DOTTALK_WITH_INDEX}>
)

if (DOTTALK_WITH_INDEX AND TARGET xindex)
  target_link_libraries(xbase PUBLIC xindex)
endif()

if (MSVC)
  target_compile_options(xbase PRIVATE /W4 /permissive-)
else()
  target_compile_options(xbase PRIVATE -Wall -Wextra -Wpedantic)
endif()
