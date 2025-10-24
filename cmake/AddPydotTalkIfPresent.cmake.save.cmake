# cmake/AddPydotTalkIfPresent.cmake
# Builds the pydottalk module if bindings/pydottalk exists, and links to xbase (+xindex optionally).

# Guard: only proceed if the directory exists
set(_PYDT_DIR "${CMAKE_SOURCE_DIR}/bindings/pydottalk")
if (NOT EXISTS "${_PYDT_DIR}/CMakeLists.txt" AND NOT EXISTS "${_PYDT_DIR}/module.cpp")
  # Fallback: simple inline build if there's no nested CMake
  if (EXISTS "${_PYDT_DIR}/module.cpp")
    # Try to find pybind11 via vcpkg
    find_package(pybind11 CONFIG QUIET)
    if (NOT pybind11_FOUND)
      message(STATUS "pydottalk: pybind11 not found via CONFIG; trying MODULE mode.")
      find_package(pybind11 QUIET)
    endif()

    if (pybind11_FOUND)
      pybind11_add_module(pydottalk "${_PYDT_DIR}/module.cpp" "${_PYDT_DIR}/xbase_glue_user.cpp")

      # Include dirs (if your headers live here)
      if (EXISTS "${CMAKE_SOURCE_DIR}/include")
        target_include_directories(pydottalk PRIVATE "${CMAKE_SOURCE_DIR}/include")
      endif()

      # Link to xbase always
      if (TARGET xbase)
        target_link_libraries(pydottalk PRIVATE xbase)
      endif()

      # Optional copy of sample data next to build (if you keep samples under bindings/pydottalk/data)
      if (EXISTS "${_PYDT_DIR}/data")
        add_custom_command(TARGET pydottalk POST_BUILD
          COMMAND ${CMAKE_COMMAND} -E copy_directory
                  "${_PYDT_DIR}/data"
                  "$<TARGET_FILE_DIR:pydottalk>/../data"  # one level up for MSVC multi-config
          COMMENT "Copying pydottalk sample data..."
        )
      endif()
    else()
      message(WARNING "pydottalk: pybind11 not found; Python module will NOT be built.")
    endif()
  endif()
else()
  # If bindings/pydottalk has its own CMakeLists, include it normally
  add_subdirectory("${CMAKE_SOURCE_DIR}/bindings/pydottalk" bindings_pydottalk)
endif()
