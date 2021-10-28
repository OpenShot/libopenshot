# SPDX-FileCopyrightText: 2021 OpenShot Studios, LLC
#
# SPDX-License-Identifier: LGPL-3.0-or-later

function(_babl_GET_VERSION _header)
  file(STRINGS "${_header}" _version_defs
    REGEX "^[ \t]*#define[ \t]+BABL_[A-Z]+_VERSION.*")
  if(_version_defs)
    string(REGEX REPLACE
      ".*BABL_MAJOR_VERSION[ \t]+([0-9]+).*" "\\1"
      babl_MAJOR "${_version_defs}")
    string(REGEX REPLACE
      ".*BABL_MINOR_VERSION[ \t]+([0-9]+).*" "\\1"
      babl_MINOR "${_version_defs}")
    string(REGEX REPLACE
      ".*BABL_MICRO_VERSION[ \t]+([0-9]+).*" "\\1"
      babl_PATCH "${_version_defs}")
    set(babl_VERSION "${babl_MAJOR}.${babl_MINOR}.${babl_PATCH}" PARENT_SCOPE)
  endif()
endfunction()

find_package(PkgConfig)
pkg_check_modules(PC_BABL babl)

set(babl_VERSION ${PC_BABL_VERSION})

find_path(babl_INCLUDE_DIR
  NAMES babl/babl.h
  PATH_SUFFIXES babl-0.1
  HINTS
    ${babl_DIR}/include
    ${PC_BABL_INCLUDE_DIRS}
  DOC "babl include dir"
)

find_library(babl_LIBRARY
  NAMES babl-0.1
  HINTS
    ${babl_DIR}/lib
    ${PC_BABL_LIBDIR}
    ${PC_BABL_LIBRARY_DIRS}
  DOC "babl library"
)

set ( babl_LIBRARIES ${babl_LIBRARY} )
set ( babl_INCLUDE_DIRS ${babl_INCLUDE_DIR} )

if(babl_INCLUDE_DIR AND NOT babl_VERSION)
  set(_version_hdr "${babl_INCLUDE_DIR}/babl/babl-version.h")
  if(EXISTS "${_version_hdr}")
    _babl_GET_VERSION("${_version_hdr}")
  endif()
endif()

if (babl_INCLUDE_DIRS AND babl_LIBRARIES)
  set(babl_FOUND TRUE)
endif()

if(babl_FOUND AND NOT TARGET babl_lib)
	add_library(babl_lib UNKNOWN IMPORTED)

	set_property(TARGET babl_lib PROPERTY
		INTERFACE_INCLUDE_DIRECTORIES ${babl_INCLUDE_DIR})
	set_property(TARGET babl_lib PROPERTY
		IMPORTED_LOCATION ${babl_LIBRARY})
endif()

include ( FindPackageHandleStandardArgs )
# handle the QUIETLY and REQUIRED arguments and set babl_FOUND to TRUE
# if all listed variables are TRUE
find_package_handle_standard_args(babl
  REQUIRED_VARS
    babl_INCLUDE_DIR
    babl_LIBRARY
  VERSION_VAR
    babl_VERSION)
