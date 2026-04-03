# src/tv/CMakeLists.txt — Turbo Vision UI units (no main)

cmake_minimum_required(VERSION 3.21)

find_package(tvision CONFIG REQUIRED)

set(TV_UI_SOURCES
  ${CMAKE_CURRENT_SOURCE_DIR}/cmd_browsetui.cpp
  ${CMAKE_CURRENT_SOURCE_DIR}/cmd_foxpro.cpp
  ${CMAKE_CURRENT_SOURCE_DIR}/cmd_foxtalk.cpp
  ${CMAKE_CURRENT_SOURCE_DIR}/cmd_generic.cpp
  ${CMAKE_CURRENT_SOURCE_DIR}/cmd_recordview.cpp

  ${CMAKE_CURRENT_SOURCE_DIR}/foxtalk_app.cpp
  ${CMAKE_CURRENT_SOURCE_DIR}/foxtalk_cmd_input.cpp
  ${CMAKE_CURRENT_SOURCE_DIR}/foxtalk_command_window.cpp
  ${CMAKE_CURRENT_SOURCE_DIR}/foxtalk_command_input.cpp
  ${CMAKE_CURRENT_SOURCE_DIR}/foxtalk_layout.cpp
  ${CMAKE_CURRENT_SOURCE_DIR}/foxtalk_log_view.cpp
  ${CMAKE_CURRENT_SOURCE_DIR}/foxtalk_menu.cpp
  ${CMAKE_CURRENT_SOURCE_DIR}/foxtalk_message_line.cpp
  ${CMAKE_CURRENT_SOURCE_DIR}/foxtalk_output_window.cpp
  ${CMAKE_CURRENT_SOURCE_DIR}/foxtalk_redirect.cpp
  ${CMAKE_CURRENT_SOURCE_DIR}/foxtalk_shell_bridge.cpp
  ${CMAKE_CURRENT_SOURCE_DIR}/foxtalk_status.cpp
  ${CMAKE_CURRENT_SOURCE_DIR}/foxtalk_workspace_view.cpp
  ${CMAKE_CURRENT_SOURCE_DIR}/foxtalk_workspace_window.cpp
)

add_library(dottalk_tvui STATIC ${TV_UI_SOURCES})

target_compile_definitions(dottalk_tvui PRIVATE DOTTALK_TV_AVAILABLE=1)

target_include_directories(dottalk_tvui
  PRIVATE
    ${CMAKE_SOURCE_DIR}/include
)

if (TARGET xbase)
  target_link_libraries(dottalk_tvui PRIVATE xbase)
elseif (TARGET xindex)
  target_link_libraries(dottalk_tvui PRIVATE xindex)
endif()

if (TARGET memo)
  target_link_libraries(dottalk_tvui PRIVATE memo)
endif()

target_link_libraries(dottalk_tvui PRIVATE tvision::tvision)