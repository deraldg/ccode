cmake_minimum_required(VERSION 3.21)

# ============================================================
# Policies
# ============================================================

# Honor CMAKE_MSVC_RUNTIME_LIBRARY on MSVC.
if (POLICY CMP0091)
    cmake_policy(SET CMP0091 NEW)
endif()

# Honor *_ROOT hints more consistently.
if (POLICY CMP0144)
    cmake_policy(SET CMP0144 NEW)
endif()

# ============================================================
# Project
# ============================================================

project(DotTalkpp VERSION 0.6 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# Default build type for single-config generators (e.g. Ninja).
if (NOT CMAKE_CONFIGURATION_TYPES AND NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE RelWithDebInfo CACHE STRING "Build type" FORCE)
endif()

# ============================================================
# User Options
# ============================================================

option(DOTTALK_WITH_TV        "Enable Turbo Vision features" ON)
option(DOTTALK_TV_TESTS       "Build Turbo Vision probe tests" ON)
option(DOTTALK_WITH_INDEX     "Enable xindex (indexing) support" ON)
option(BUILD_PYDOTTALK        "Build Python bindings" OFF)

# Transitional / developer-only command surfaces.
option(DOTTALK_WITH_DEV       "Include developer tools and transitional aliases" ON)

# Build profile: use this to steer capacity and diagnostics defaults.
set(DOTTALK_PROFILE "DEV" CACHE STRING "Build profile: DEV or PROD")
set_property(CACHE DOTTALK_PROFILE PROPERTY STRINGS DEV PROD)

# Maximum visible work areas for the shell / CLI layer.
# This may be overridden by the profile block below.
set(DOTTALK_MAX_AREA "" CACHE STRING "Maximum open work areas (empty = choose from profile)")

# Optional extra diagnostics.
option(DOTTALK_EXTRA_DIAGNOSTICS "Enable extra internal diagnostics/logging" ON)

# ============================================================
# Profile Resolution
# ============================================================

string(TOUPPER "${DOTTALK_PROFILE}" DOTTALK_PROFILE)

if (NOT DOTTALK_PROFILE STREQUAL "DEV" AND NOT DOTTALK_PROFILE STREQUAL "PROD")
    message(FATAL_ERROR "DOTTALK_PROFILE must be DEV or PROD")
endif()

if (DOTTALK_PROFILE STREQUAL "DEV")
    if (DOTTALK_MAX_AREA STREQUAL "")
        set(DOTTALK_MAX_AREA "256" CACHE STRING "Maximum open work areas" FORCE)
    endif()

    if (NOT DEFINED DOTTALK_WITH_DEV)
        set(DOTTALK_WITH_DEV ON CACHE BOOL "Include developer tools and transitional aliases" FORCE)
    endif()

    if (NOT DEFINED DOTTALK_EXTRA_DIAGNOSTICS)
        set(DOTTALK_EXTRA_DIAGNOSTICS ON CACHE BOOL "Enable extra internal diagnostics/logging" FORCE)
    endif()
else()
    if (DOTTALK_MAX_AREA STREQUAL "")
        set(DOTTALK_MAX_AREA "128" CACHE STRING "Maximum open work areas" FORCE)
    endif()

    # Leave DOTTALK_WITH_DEV user-overridable, but default public intent is OFF.
    # Since option() already created it, only force it if caller did not explicitly set it.
    if (NOT DEFINED CACHE{DOTTALK_WITH_DEV})
        set(DOTTALK_WITH_DEV OFF CACHE BOOL "Include developer tools and transitional aliases" FORCE)
    endif()

    if (NOT DEFINED CACHE{DOTTALK_EXTRA_DIAGNOSTICS})
        set(DOTTALK_EXTRA_DIAGNOSTICS OFF CACHE BOOL "Enable extra internal diagnostics/logging" FORCE)
    endif()
endif()

# ============================================================
# Environment / Local Hints
# ============================================================

# Optional Turbo Vision local source hint.
set(_TV_ENV "$ENV{TVISION_ROOT}")
if (_TV_ENV)
    set(TVISION_ROOT "${_TV_ENV}" CACHE PATH "Local Turbo Vision source tree")
elseif (EXISTS "C:/code/tvision/CMakeLists.txt")
    set(TVISION_ROOT "C:/code/tvision" CACHE PATH "Local Turbo Vision source tree")
elseif (EXISTS "${CMAKE_SOURCE_DIR}/../tvision/CMakeLists.txt")
    set(TVISION_ROOT "${CMAKE_SOURCE_DIR}/../tvision" CACHE PATH "Local Turbo Vision source tree")
else()
    set(TVISION_ROOT "" CACHE PATH "Local Turbo Vision source tree")
endif()

# Optional vcpkg root hint.
set(_VCPKG_ENV "$ENV{VCPKG_ROOT}")
if (_VCPKG_ENV)
    set(VCPKG_ROOT "${_VCPKG_ENV}" CACHE PATH "vcpkg root (hint)")
elseif (EXISTS "C:/Users/deral/vcpkg/.vcpkg-root")
    set(VCPKG_ROOT "C:/Users/deral/vcpkg" CACHE PATH "vcpkg root (hint)")
elseif (EXISTS "C:/vcpkg/.vcpkg-root")
    set(VCPKG_ROOT "C:/vcpkg" CACHE PATH "vcpkg root (hint)")
else()
    set(VCPKG_ROOT "" CACHE PATH "vcpkg root (hint)")
endif()

# ============================================================
# MSVC / Windows Defaults
# ============================================================

if (MSVC)
    # Match CRT to vcpkg triplet when possible.
    if (DEFINED VCPKG_TARGET_TRIPLET AND VCPKG_TARGET_TRIPLET MATCHES "static")
        set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")
    else()
        set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>DLL")
    endif()

    add_compile_definitions(
        NOMINMAX
        _CRT_SECURE_NO_WARNINGS
    )
endif()

# ============================================================
# Prefix Search Hints
# ============================================================

# Prefer static triplet installs when present; keep dynamic as fallback.
# This is a local preference layer to reduce accidental cross-triplet mixing.
if (EXISTS "${CMAKE_BINARY_DIR}/vcpkg_installed/x64-windows-static")
    list(INSERT CMAKE_PREFIX_PATH 0 "${CMAKE_BINARY_DIR}/vcpkg_installed/x64-windows-static")
endif()

if (VCPKG_ROOT AND EXISTS "${VCPKG_ROOT}/installed/x64-windows-static")
    list(INSERT CMAKE_PREFIX_PATH 0 "${VCPKG_ROOT}/installed/x64-windows-static")
endif()

if (EXISTS "${CMAKE_BINARY_DIR}/vcpkg_installed/x64-windows")
    list(APPEND CMAKE_PREFIX_PATH "${CMAKE_BINARY_DIR}/vcpkg_installed/x64-windows")
endif()

if (VCPKG_ROOT AND EXISTS "${VCPKG_ROOT}/installed/x64-windows")
    list(APPEND CMAKE_PREFIX_PATH "${VCPKG_ROOT}/installed/x64-windows")
endif()

# ============================================================
# Main Source Tree
# ============================================================

add_subdirectory(src)

# Add Python binding helper after code targets exist.
include(cmake/AddPydotTalkIfPresent.cmake OPTIONAL)

# ============================================================
# Turbo Vision Detection
# ============================================================

set(_tvision_found OFF)
set(_tvision_origin "not-found")

if (DOTTALK_WITH_TV)
    # First choice: config package from vcpkg or explicit tvision_DIR.
    find_package(tvision CONFIG QUIET)

    # Normalize alias if package exports tvision::tvision.
    if (TARGET tvision::tvision AND NOT TARGET tvision)
        add_library(tvision INTERFACE IMPORTED)
        target_link_libraries(tvision INTERFACE tvision::tvision)
    endif()

    if (TARGET tvision)
        set(_tvision_found ON)
        set(_tvision_origin "config-package")
    endif()

    # Fallback: local source tree.
    if (NOT _tvision_found AND TVISION_ROOT AND EXISTS "${TVISION_ROOT}/CMakeLists.txt")
        message(STATUS "Turbo Vision: using local source at ${TVISION_ROOT}")
        add_subdirectory("${TVISION_ROOT}" _tvision_local EXCLUDE_FROM_ALL)

        if (TARGET tvision)
            set(_tvision_found ON)
            set(_tvision_origin "local-source")
        endif()
    endif()
endif()

# ============================================================
# SQLite Detection
# ============================================================

set(_sqlite_found OFF)
set(_sqlite_origin "not-found")

find_package(SQLite3 QUIET)
if (SQLite3_FOUND)
    set(_sqlite_found ON)
    set(_sqlite_origin "official")
else()
    find_package(unofficial-sqlite3 CONFIG QUIET)
    if (TARGET unofficial::sqlite3::sqlite3)
        set(_sqlite_found ON)
        set(_sqlite_origin "unofficial")
    endif()
endif()

# ============================================================
# Target Helper
# ============================================================

function(dottalk_apply_common_settings target_name)
    if (NOT TARGET ${target_name})
        return()
    endif()

    # Profile / capacity / diagnostics
    target_compile_definitions(${target_name} PRIVATE
        DOTTALK_PROFILE_${DOTTALK_PROFILE}=1
        DOTTALK_MAX_AREA=${DOTTALK_MAX_AREA}
    )

    if (DOTTALK_EXTRA_DIAGNOSTICS)
        target_compile_definitions(${target_name} PRIVATE DOTTALK_EXTRA_DIAGNOSTICS=1)
    else()
        target_compile_definitions(${target_name} PRIVATE DOTTALK_EXTRA_DIAGNOSTICS=0)
    endif()

    # Developer surfaces
    if (DOTTALK_WITH_DEV)
        target_compile_definitions(${target_name} PRIVATE DOTTALK_WITH_DEV=1)
    else()
        target_compile_definitions(${target_name} PRIVATE DOTTALK_WITH_DEV=0)
    endif()

    # Indexing
    if (DOTTALK_WITH_INDEX)
        target_compile_definitions(${target_name} PRIVATE DOTTALK_WITH_INDEX=1)
        if (TARGET xindex)
            target_link_libraries(${target_name} PRIVATE xindex)
        endif()
    else()
        target_compile_definitions(${target_name} PRIVATE DOTTALK_WITH_INDEX=0)
    endif()

    # TVision
    if (_tvision_found)
        target_link_libraries(${target_name} PRIVATE tvision)
        target_compile_definitions(${target_name} PRIVATE DOTTALK_TV_AVAILABLE=1)
    else()
        target_compile_definitions(${target_name} PRIVATE DOTTALK_TV_AVAILABLE=0)
    endif()

    # SQLite
    if (_sqlite_found)
        if (SQLite3_FOUND)
            target_link_libraries(${target_name} PRIVATE SQLite::SQLite3)
        else()
            target_link_libraries(${target_name} PRIVATE unofficial::sqlite3::sqlite3)
        endif()
        target_compile_definitions(${target_name} PRIVATE DOTTALK_SQLITE_AVAILABLE=1)
    else()
        target_compile_definitions(${target_name} PRIVATE DOTTALK_SQLITE_AVAILABLE=0)
    endif()
endfunction()

# ============================================================
# Apply Target Settings
# ============================================================

if (TARGET dottalkpp)
    dottalk_apply_common_settings(dottalkpp)

    if (_tvision_found)
        message(STATUS "Turbo Vision ENABLED for 'dottalkpp' via ${_tvision_origin}.")
    elseif (DOTTALK_WITH_TV)
        message(WARNING "Turbo Vision NOT found; 'dottalkpp' will build WITHOUT TV.")
    else()
        message(STATUS "Turbo Vision explicitly disabled for 'dottalkpp'.")
    endif()

    if (DOTTALK_WITH_INDEX)
        message(STATUS "Indexing ENABLED for 'dottalkpp'.")
    else()
        message(STATUS "Indexing DISABLED for 'dottalkpp'.")
    endif()

    if (DOTTALK_WITH_DEV)
        message(STATUS "Developer tools ENABLED for 'dottalkpp'.")
    else()
        message(STATUS "Developer tools DISABLED for 'dottalkpp'.")
    endif()
endif()

if (BUILD_PYDOTTALK)
    if (TARGET pydottalk)
        dottalk_apply_common_settings(pydottalk)

        if (DOTTALK_WITH_INDEX)
            message(STATUS "Indexing ENABLED for 'pydottalk'.")
        else()
            message(STATUS "Indexing DISABLED for 'pydottalk'.")
        endif()
    else()
        message(WARNING "BUILD_PYDOTTALK=ON but target 'pydottalk' was not created.")
    endif()
endif()

# ============================================================
# Tests
# ============================================================

include(CTest)
if (BUILD_TESTING)
    add_subdirectory(src/tests)
endif()

# ============================================================
# Summary
# ============================================================

message(STATUS "==== DotTalk++ / Build summary ====")
message(STATUS "  CMAKE_BUILD_TYPE      : ${CMAKE_BUILD_TYPE}")
message(STATUS "  DOTTALK_PROFILE       : ${DOTTALK_PROFILE}")
message(STATUS "  DOTTALK_MAX_AREA      : ${DOTTALK_MAX_AREA}")
message(STATUS "  DOTTALK_WITH_DEV      : ${DOTTALK_WITH_DEV}")
message(STATUS "  DOTTALK_EXTRA_DIAGN.  : ${DOTTALK_EXTRA_DIAGNOSTICS}")
message(STATUS "  DOTTALK_WITH_INDEX    : ${DOTTALK_WITH_INDEX}")
message(STATUS "  DOTTALK_WITH_TV       : ${DOTTALK_WITH_TV}")
message(STATUS "  DOTTALK_TV_TESTS      : ${DOTTALK_TV_TESTS}")
message(STATUS "  BUILD_PYDOTTALK       : ${BUILD_PYDOTTALK}")
message(STATUS "  BUILD_TESTING         : ${BUILD_TESTING}")
message(STATUS "  TVISION_ROOT          : ${TVISION_ROOT}")
message(STATUS "  VCPKG_ROOT            : ${VCPKG_ROOT}")
message(STATUS "  sqlite found          : ${_sqlite_found}")
message(STATUS "  sqlite origin         : ${_sqlite_origin}")
message(STATUS "  tvision found         : ${_tvision_found}")
message(STATUS "  tvision origin        : ${_tvision_origin}")
message(STATUS "===================================")