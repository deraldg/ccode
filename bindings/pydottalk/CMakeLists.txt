cmake_minimum_required(VERSION 3.21)

# Keep policy modern so *_ROOT hints work consistently.
if(POLICY CMP0144)
  cmake_policy(SET CMP0144 NEW)
endif()

# ---- Python & pybind11 ----
find_package(Python3 COMPONENTS Interpreter Development REQUIRED)

# Help CMake find pybind11 in common places:
# - Local manifest tree: <repo>/vcpkg_installed/x64-windows/share/pybind11
# - VS-integrated vcpkg: %VCPKG_ROOT%/installed/x64-windows/share/pybind11
set(_pybind11_hint_dirs
  "${CMAKE_SOURCE_DIR}/vcpkg_installed/${VCPKG_TARGET_TRIPLET}/share/pybind11"
  "${CMAKE_SOURCE_DIR}/vcpkg_installed/x64-windows/share/pybind11"
)
if(DEFINED ENV{pybind11_DIR})
  list(APPEND _pybind11_hint_dirs "$ENV{pybind11_DIR}")
endif()
foreach(_d IN LISTS _pybind11_hint_dirs)
  if(EXISTS "${_d}/pybind11Config.cmake")
    list(APPEND CMAKE_PREFIX_PATH "${_d}/..") # parent of 'share/pybind11'
  endif()
endforeach()

find_package(pybind11 CONFIG QUIET)
if(NOT pybind11_FOUND)
  find_package(pybind11 REQUIRED)
endif()

# ---- Module target ----
pybind11_add_module(pydottalk
  src/module.cpp
)

# If your core target exists, link to it and advertise to C++ via a macro.
if(TARGET xbase)
  target_link_libraries(pydottalk PRIVATE xbase)
  target_compile_definitions(pydottalk PRIVATE HAVE_XBASE=1)
else()
  message(WARNING "Target 'xbase' not found; building pydottalk without xbase for now.")
endif()

target_compile_features(pydottalk PRIVATE cxx_std_20)
set_target_properties(pydottalk PROPERTIES
  CXX_VISIBILITY_PRESET "default"
  VISIBILITY_INLINES_HIDDEN OFF
)

# Put the .pyd in build/python for ALL configs (VS multi-config friendly)
foreach(cfg IN ITEMS Debug Release RelWithDebInfo MinSizeRel)
  string(TOUPPER "${cfg}" CFGU)
  set_target_properties(pydottalk PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY_${CFGU} "${CMAKE_BINARY_DIR}/python"
    LIBRARY_OUTPUT_DIRECTORY_${CFGU} "${CMAKE_BINARY_DIR}/python"
    ARCHIVE_OUTPUT_DIRECTORY_${CFGU} "${CMAKE_BINARY_DIR}/python"
  )
endforeach()

# Optional install rule
include(GNUInstallDirs)
install(TARGETS pydottalk
  RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
  LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
  ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR}
)
