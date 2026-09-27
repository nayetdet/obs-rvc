# CMake Linux defaults module

include_guard(GLOBAL)

# Set default installation directories
include(GNUInstallDirs)

if(CMAKE_INSTALL_LIBDIR MATCHES "(CMAKE_SYSTEM_PROCESSOR)")
  string(REPLACE "CMAKE_SYSTEM_PROCESSOR" "${CMAKE_SYSTEM_PROCESSOR}" CMAKE_INSTALL_LIBDIR "${CMAKE_INSTALL_LIBDIR}")
endif()

# Enable find_package targets to become globally available targets
set(CMAKE_FIND_PACKAGE_TARGETS_GLOBAL TRUE)

set(CPACK_PACKAGE_NAME "${CMAKE_PROJECT_NAME}")
set(CPACK_PACKAGE_VERSION "${CMAKE_PROJECT_VERSION}")
set(CPACK_PACKAGE_FILE_NAME "${CPACK_PACKAGE_NAME}-${CPACK_PACKAGE_VERSION}-${CMAKE_C_LIBRARY_ARCHITECTURE}")

set(CPACK_GENERATOR "DEB")
set(CPACK_DEBIAN_PACKAGE_SHLIBDEPS OFF)
set(CPACK_DEBIAN_PACKAGE_DEPENDS "obs-studio")
set(CPACK_DEBIAN_PACKAGE_MAINTAINER "${PLUGIN_EMAIL}")
set(CPACK_SET_DESTDIR ON)
set(CPACK_DEBIAN_DEBUGINFO_PACKAGE OFF)

set(CPACK_OUTPUT_FILE_PREFIX "${CMAKE_CURRENT_SOURCE_DIR}/release")

set(CPACK_SOURCE_GENERATOR "TXZ")
set(
  CPACK_SOURCE_IGNORE_FILES
  ".*~$"
  \\.git/
  \\.github/
  \\.gitignore
  \\.ccache/
  build_.*
  cmake/\\.CMakeBuildNumber
  release/
)

set(CPACK_VERBATIM_VARIABLES YES)
set(CPACK_SOURCE_PACKAGE_FILE_NAME "${CPACK_PACKAGE_NAME}-${CPACK_PACKAGE_VERSION}-source")
set(CPACK_ARCHIVE_THREADS 0)

include(CPack)

find_package(libobs QUIET)

if(NOT TARGET OBS::libobs)
  find_package(LibObs REQUIRED)
  add_library(OBS::libobs ALIAS libobs)

  macro(find_package)
    if(NOT "${ARGV0}" STREQUAL libobs)
      _find_package(${ARGV})
    endif()
  endmacro()
endif()
