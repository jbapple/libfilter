add_library(libfilter_cxx INTERFACE)
add_library(libfilter::cxx ALIAS libfilter_cxx)

target_link_libraries(libfilter_cxx INTERFACE libfilter_internal)
target_link_libraries(libfilter_cxx INTERFACE libfilter_c)

target_include_directories(
  libfilter_cxx
  INTERFACE $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
            $<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}>)

set_target_properties(libfilter_cxx PROPERTIES EXPORT_NAME cxx)

# Install libfilter_c in PREFIX/lib and headers in PREFIX/include/filter.
install(
  TARGETS libfilter_cxx
  EXPORT libfilterTargets
  ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR}
  LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
  RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR})

install(
  DIRECTORY include/filter
  DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}
  FILES_MATCHING
  PATTERN "*.hpp")
