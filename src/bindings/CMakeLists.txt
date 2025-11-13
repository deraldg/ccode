# After add_library(pydottalk MODULE ...)

if (TARGET xbase)
  target_link_libraries(pydottalk PRIVATE xbase)
elseif (TARGET xindex)
  target_link_libraries(pydottalk PRIVATE xindex)
endif()

target_include_directories(pydottalk PRIVATE ${CMAKE_SOURCE_DIR}/include)
target_compile_definitions(pydottalk PRIVATE DOTTALK_WITH_INDEX=$<BOOL:${DOTTALK_WITH_INDEX}>)
