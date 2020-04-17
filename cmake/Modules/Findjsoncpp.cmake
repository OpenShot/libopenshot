# - Try to find jsoncpp
#
# IMPORTED target
#  This module will create the target jsoncpp_lib if jsoncpp is found
#
# Legacy Config Variables
#  The following variables are defined for backwards compatibility:
#
#  JSONCPP_INCLUDE_DIRS, where to find header, etc.
#  JSONCPP_LIBRARIES, the libraries needed to use jsoncpp.
#  JSONCPP_FOUND, If false, do not try to use jsoncpp.
#  JSONCPP_INCLUDE_PREFIX, include prefix for jsoncpp

# try to detect using pkg-config, and use as hints later
find_package(PkgConfig)
pkg_check_modules(PC_jsoncpp QUIET jsoncpp)

find_path(
  jsoncpp_INCLUDE_DIR
  NAMES json/json.h
  HINTS ${PC_jsoncpp_INCLUDE_DIRS}
  DOC "jsoncpp include dir"
)

find_library(
  jsoncpp_LIBRARY
  NAMES jsoncpp
  HINTS ${PC_jsoncpp_LIBRARY_DIR}
  DOC "jsoncpp library"
)

set(jsoncpp_INCLUDE_DIRS ${jsoncpp_INCLUDE_DIR})
set(jsoncpp_LIBRARIES ${jsoncpp_LIBRARY})

if (jsoncpp_INCLUDE_DIRS AND jsoncpp_LIBRARIES)
  set(jsoncpp_FOUND TRUE)
endif()

# Create the IMPORTED target
if (jsoncpp_FOUND AND NOT TARGET jsoncpp_lib)
  add_library(jsoncpp_lib UNKNOWN IMPORTED)

  set_property(TARGET jsoncpp_lib PROPERTY
    INTERFACE_INCLUDE_DIRECTORIES ${jsoncpp_INCLUDE_DIR})

  set_property(TARGET jsoncpp_lib PROPERTY
    IMPORTED_LOCATION ${jsoncpp_LIBRARY})
endif()

# debug library on windows
# same naming convention as in qt (appending debug library with d)
# boost is using the same "hack" as us with "optimized" and "debug"
if ("${CMAKE_CXX_COMPILER_ID}" STREQUAL "MSVC")
  find_library(
    jsoncpp_LIBRARY_DEBUG
    NAMES jsoncppd
    HINTS ${PC_jsoncpp_LIBDIR} ${PC_jsoncpp_LIBRARY_DIRS}
    DOC "jsoncpp debug library"
  )

  set(jsoncpp_LIBRARIES optimized ${jsoncpp_LIBRARIES} debug ${jsoncpp_LIBRARY_DEBUG})

  # Add Debug location to IMPORTED target
  if(TARGET jsoncpp_lib)
    set_property(TARGET jsoncpp_lib APPEND PROPERTY
      IMPORTED_LOCATION_Debug ${jsoncpp_LIBRARY_DEBUG})
  endif()
endif()

# find jsoncpp_INCLUDE_PREFIX
find_path(
  jsoncpp_INCLUDE_PREFIX
  NAMES json.h
  HINTS ${jsoncpp_INCLUDE_DIR}
  PATH_SUFFIXES jsoncpp/json json
)

if (${jsoncpp_INCLUDE_PREFIX} MATCHES "jsoncpp")
  set(jsoncpp_INCLUDE_PREFIX "jsoncpp/json")
else()
  set(jsoncpp_INCLUDE_PREFIX "json")
endif()

# Check the available version
set(_version_file "${jsoncpp_INCLUDE_DIR}/${jsoncpp_INCLUDE_PREFIX}/version.h")
if (jsoncpp_INCLUDE_DIR AND EXISTS ${_version_file})
  file(STRINGS "${_version_file}" jsoncpp_version_str
    REGEX "JSONCPP_VERSION_STRING.*\"[^\"]+\"")
  if(jsoncpp_version_str MATCHES "JSONCPP_VERSION_STRING.*\"([^\"]+)\"")
    set(jsoncpp_VERSION_STRING ${CMAKE_MATCH_1})
  endif()
  unset(jsoncpp_version_str)
  string(REGEX REPLACE "^([0-9]+\.[0-9]+\.[0-9]+).*$" "\\1"
    jsoncpp_VERSION "${jsoncpp_VERSION_STRING}")
endif()

if(NOT jsoncpp_VERSION)
  unset(jsoncpp_VERSION)
  unset(jsoncpp_VERSION_STRING)
endif()

# Check version requirement, if specified
if(jsoncpp_FIND_VERSION AND jsoncpp_VERSION)
  if("${jsoncpp_FIND_VERSION}" STREQUAL "${jsoncpp_VERSION}")
    set(jsoncpp_VERSION_EXACT TRUE)
  endif()
  if("${jsoncpp_FIND_VERSION}" VERSION_GREATER "${jsoncpp_VERSION}")
    set(jsoncpp_VERSION_COMPATIBLE FALSE)
  else()
    set(jsoncpp_VERSION_COMPATIBLE TRUE)
  endif()
endif()

# Legacy
set(JSONCPP_LIBRARY ${jsoncpp_LIBRARY})
set(JSONCPP_LIBRARIES ${jsoncpp_LIBRARIES})
set(JSONCPP_INCLUDE_DIR ${jsoncpp_INCLUDE_DIR})
set(JSONCPP_INCLUDE_DIRS ${jsoncpp_INCLUDE_DIRS})
set(JSONCPP_INCLUDE_PREFIX ${jsoncpp_INCLUDE_PREFIX})
set(JSONCPP_VERSION ${jsoncpp_VERSION})
set(JSONCPP_FOUND ${jsoncpp_FOUND})

# handle the QUIETLY and REQUIRED arguments and set jsoncpp_FOUND to TRUE
# if all listed variables are TRUE, hide their existence from configuration view
include(FindPackageHandleStandardArgs)
find_package_handle_standard_args(jsoncpp
  REQUIRED_VARS
    jsoncpp_INCLUDE_DIR
    jsoncpp_LIBRARY
  VERSION_VAR
    jsoncpp_VERSION
)
mark_as_advanced (jsoncpp_INCLUDE_DIR jsoncpp_LIBRARY)
