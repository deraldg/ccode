cmake_minimum_required(VERSION 3.20)
project(dottalk_tui CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

include(FetchContent)

# Fetch Turbo Vision
FetchContent_Declare(
  tvision
  GIT_REPOSITORY https://github.com/magiblot/tvision.git
  GIT_TAG master
)
FetchContent_MakeAvailable(tvision)

add_executable(dottalk_tui
  src/main.cpp
  src/CommandWindow.cpp
)

# app includes
target_include_directories(dottalk_tui PRIVATE
  "${tvision_SOURCE_DIR}/include"
  "${CMAKE_CURRENT_SOURCE_DIR}/src"
)

target_link_libraries(dottalk_tui PRIVATE tvision)

if (MSVC)
  target_compile_definitions(dottalk_tui PRIVATE _CRT_SECURE_NO_WARNINGS)
  target_compile_options(dottalk_tui PRIVATE /W4 /permissive- /Zc:__cplusplus)
else()
  target_compile_options(dottalk_tui PRIVATE -Wall -Wextra -Wpedantic)
endif()
