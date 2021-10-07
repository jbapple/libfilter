cmake_minimum_required(VERSION 3.15...3.21 FATAL_ERROR)

# -- project setup -------------------------------------------------------------

project(
  libfilter
  VERSION 0.1.0
  DESCRIPTION "High-speed Bloom filters and taffy filters for C and C++"
  HOMEPAGE_URL "https://github.com/tenzir/libfilter"
  LANGUAGES C CXX)

# -- sanity checks -------------------------------------------------------------

# Prohibit in-source builds.
if ("${CMAKE_SOURCE_DIR}" STREQUAL "${CMAKE_BINARY_DIR}")
  message(FATAL_ERROR "In-source builds are not allowed.")
endif ()

# Ensure that CMAKE_INSTALL_PREFIX is not a relative path.
if (NOT IS_ABSOLUTE "${CMAKE_INSTALL_PREFIX}")
  message(
    FATAL_ERROR
      "CMAKE_INSTALL_PREFIX must be an absolute path: ${CMAKE_INSTALL_PREFIX}")
endif ()

# -- includes ------------------------------------------------------------------

include(CMakePackageConfigHelpers)
include(CMakePrintHelpers)
include(GNUInstallDirs)

# -- project configuration -----------------------------------------------------

# Determine whether we're building libfilter as a subproject. This is used to
# determine good default values for many options. libfilter should not modify
# global CMake if built as a subproject, unless explicitly requested to do so
# with options.
get_directory_property(_libfilter_PARENT_DIRECTORY PARENT_DIRECTORY)
if (_libfilter_PARENT_DIRECTORY)
  set(libfilter_IS_SUBPROJECT ON)
  set(libfilter_IS_NOT_SUBPROJECT OFF)
else ()
  set(libfilter_IS_SUBPROJECT OFF)
  set(libfilter_IS_NOT_SUBPROJECT ON)
endif ()
unset(_libfilter_PARENT_DIRECTORY)

# -- internal target -----------------------------------------------------------

# Create the internal target that is used for option/property propagation.
add_library(libfilter_internal INTERFACE)
set_target_properties(libfilter_internal PROPERTIES EXPORT_NAME internal)
add_library(libfilter::internal ALIAS libfilter_internal)
install(TARGETS libfilter_internal EXPORT libfilterTargets)

# Require standard C++20.
target_compile_features(libfilter_internal INTERFACE cxx_std_20)

# Increase maximum number of template instantiations.
target_compile_options(libfilter_internal INTERFACE
  $<$<COMPILE_LANG_AND_ID:CXX,AppleClang,Clang,GCC>:-ftemplate-backtrace-limit=0>)

# -- FreeBSD support -----------------------------------------------------------

if (${CMAKE_SYSTEM_NAME} MATCHES "FreeBSD")
  # Works around issues with libstdc++ and C++11. For details, see: -
  # https://bugs.freebsd.org/bugzilla/show_bug.cgi?id=194929 -
  # https://bugs.freebsd.org/bugzilla/show_bug.cgi?id=182657
  target_compile_definitions(
    libfilter_internal INTERFACE _GLIBCXX_USE_C99 _GLIBCXX_USE_C99_MATH
                                 _GLIBCXX_USE_C99_MATH_TR1)
endif ()

# -- compile options -----------------------------------------------------------

target_compile_options(
  libfilter_internal
  INTERFACE $<$<CONFIG:Release>:-fno-omit-frame-pointer>
            $<$<CONFIG:RelWithDebInfo>:-fno-omit-frame-pointer>
            $<$<CONFIG:Debug>:-O0>
            -msse3 -mssse3 -msse4.1 -msse4.2 -mavx -mavx2 -mfma)

# -- developer mode ------------------------------------------------------------

# Build libfilter in developer mode. This is enabled by default when not building
# libfilter as a subproject. The developer mode contains CCache support and many
# other niceties.
option(libfilter_ENABLE_DEVELOPER_MODE
       "Enables build settings for a nicer development environment"
       "${libfilter_IS_NOT_SUBPROJECT}")

if (libfilter_ENABLE_DEVELOPER_MODE)
  # Support tools like clang-tidy by creating a compilation database and
  # copying it to the project root.
  set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

  # Force colored output for the Ninja generator
  if ("${CMAKE_GENERATOR}" MATCHES "Ninja")
    if ("${CMAKE_CXX_COMPILER_ID}" STREQUAL "GNU")
      target_compile_options(libfilter_internal
                             INTERFACE -fdiagnostics-color=always)
    elseif ("${CMAKE_CXX_COMPILER_ID}" MATCHES "Clang")
      target_compile_options(libfilter_internal INTERFACE -fcolor-diagnostics)
    endif ()
  endif ()

  # Keep make output sane
  set(CMAKE_VERBOSE_MAKEFILE
      OFF
      CACHE STRING "Show all outputs including compiler lines." FORCE)
endif ()

if (libfilter_ENABLE_DEVELOPER_MODE OR libfilter_ENABLE_BUILDID)
  # Relocate debug paths to a common prefix for CCache users that work from
  # multiple worktrees.
  # The debug paths affect the build-id, we rewrite them to get a more
  # reproducible build.
  target_compile_options(
    libfilter_internal
    INTERFACE "-fdebug-prefix-map=${CMAKE_CURRENT_SOURCE_DIR}=.")
endif ()


add_subdirectory(c)
add_subdirectory(cpp)

# -- cmake export/config installations -----------------------------------------

export(
  EXPORT libfilterTargets
  FILE libfilterTargets.cmake
  NAMESPACE libfilter::)

install(
  EXPORT libfilterTargets
  DESTINATION "${CMAKE_INSTALL_LIBDIR}/cmake/libfilter"
  NAMESPACE libfilter::)

write_basic_package_version_file(
  "${CMAKE_CURRENT_BINARY_DIR}/libfilterConfigVersion.cmake"
  VERSION "${libfilter_VERSION_MAJOR}.${libfilter_VERSION_MINOR}.${libfilter_VERSION_PATCH}"
  COMPATIBILITY ExactVersion)

configure_package_config_file(
  "${CMAKE_CURRENT_SOURCE_DIR}/cmake/libfilterConfig.cmake.in"
  "${CMAKE_CURRENT_BINARY_DIR}/libfilterConfig.cmake"
  INSTALL_DESTINATION "${CMAKE_INSTALL_LIBDIR}/cmake/libfilter")

install(
  FILES "${CMAKE_CURRENT_BINARY_DIR}/libfilterConfig.cmake"
        "${CMAKE_CURRENT_BINARY_DIR}/libfilterConfigVersion.cmake"
  DESTINATION "${CMAKE_INSTALL_LIBDIR}/cmake/libfilter")
