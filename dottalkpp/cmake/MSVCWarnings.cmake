# cmake/MSVCWarnings.cmake
#
# Usage:
#   include(cmake/MSVCWarnings.cmake)
#   target_enable_warnings(your_target)

function(target_enable_warnings target_name)
  if (MSVC)
    target_compile_options(${target_name} PRIVATE
      /W4
      /permissive-
      /Zc:__cplusplus
      /Zc:preprocessor
      /EHsc
      /utf-8
    )
    # Optional: treat warnings as errors.
    # target_compile_options(${target_name} PRIVATE /WX)

    # If you prefer not to add (void)param, you can disable unused-parameter warnings:
    # target_compile_options(${target_name} PRIVATE /wd4100)
  else()
    target_compile_options(${target_name} PRIVATE
      -Wall
      -Wextra
      -Wpedantic
    )
    # Optional: treat warnings as errors.
    # target_compile_options(${target_name} PRIVATE -Werror)
  endif()
endfunction()
