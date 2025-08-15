# --- XBASE library ----------------------------------------------------------
# Collect xbase sources (adjust the globs/paths if your files live elsewhere)
file(GLOB_RECURSE XBASE_SOURCES
     "${CMAKE_CURRENT_SOURCE_DIR}/src/xbase/*.cpp"
)

if (XBASE_SOURCES)
  add_library(xbase STATIC ${XBASE_SOURCES})
  target_include_directories(xbase PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}/include")
  # If xbase code calls into xindex, keep this:
  target_link_libraries(xbase PUBLIC xindex)
else()
  message(FATAL_ERROR "Wanted to build 'xbase' but found no sources in src/xbase/*.cpp")
endif()
