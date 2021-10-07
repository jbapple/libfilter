set(sources
  lib/block.c
  lib/memory.c
  lib/util.c)

add_library(libfilter_c ${sources})
add_library(libfilter::c ALIAS libfilter_c)

target_link_libraries(libfilter_c PRIVATE libfilter_internal)

target_include_directories(
  libfilter_c
  PUBLIC $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
         $<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}>)

set_target_properties(libfilter_c PROPERTIES EXPORT_NAME c
                                             OUTPUT_NAME filter)

# Install libfilter_c in PREFIX/lib and headers in PREFIX/include/filter.
install(
  TARGETS libfilter_c
  EXPORT libfilterTargets
  ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR}
  LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
  RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR})

install(
  DIRECTORY include/filter
  DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}
  FILES_MATCHING
  PATTERN "*.h")
