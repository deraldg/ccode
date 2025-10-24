# cmake/AddPydotTalkIfPresent.cmake
# Guarded wiring for the optional pydottalk binding.

option(BUILD_PYDOTTALK "Build Python bindings for DotTalk++ (pybind11)" OFF)

set(_pyd_bind_dir "${CMAKE_SOURCE_DIR}/bindings/pydottalk")
if(BUILD_PYDOTTALK)
    if (EXISTS "${_pyd_bind_dir}/CMakeLists.txt")
        message(STATUS "Enabling Python binding: ${_pyd_bind_dir}")
        add_subdirectory("${_pyd_bind_dir}")
    else()
        message(STATUS "BUILD_PYDOTTALK=ON but ${_pyd_bind_dir} not found; skipping.")
    endif()
else()
    message(STATUS "BUILD_PYDOTTALK=OFF; skipping Python binding.")
endif()
