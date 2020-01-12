# vim: ts=2 sw=2
#[=======================================================================[.rst:
FindRESVG
---------
Try to find the shared-library build of resvg, the Rust SVG library

IMPORTED targets
^^^^^^^^^^^^^^^^

This module defines :prop_tgt:`IMPORTED` target ``RESVG::resvg`` when
the library and headers are found.

Result Variables
^^^^^^^^^^^^^^^^

This module defines the following variables:

::

  RESVG_FOUND         - Library and header files found
  RESVG_INCLUDE_DIRS  - Include directory path
  RESVG_LIBRARIES     - Link path to the library
  RESVG_DEFINITIONS   - Compiler switches (currently unused)

Backwards compatibility
^^^^^^^^^^^^^^^^^^^^^^^

For compatibility with previous versions of this module, uppercase names
for FFmpeg and for all components are also recognized, and all-uppercase
versions of the cache variables are also created.

Control variables
^^^^^^^^^^^^^^^^^

The following variables can be used to provide path hints to the module:

RESVGDIR      - Set in the calling CMakeLists.txt or on the command line
ENV{RESVGDIR} - An environment variable in the cmake process context

Copyright (c) 2020, FeRD (Frank Dana) <ferdnyc@gmail.com>
#]=======================================================================]
include(FindPackageHandleStandardArgs)

# CMake 3.4+ only: Convert relative paths to absolute
if(DEFINED RESVGDIR AND CMAKE_VERSION VERSION_GREATER 3.4)
  get_filename_component(RESVGDIR "${RESVGDIR}" ABSOLUTE
    BASE_DIR ${CMAKE_CURRENT_BINARY_DIR})
endif()

find_path(RESVG_INCLUDE_DIRS
  ResvgQt.h
  PATHS
    ${RESVGDIR}
    ${RESVGDIR}/include
    $ENV{RESVGDIR}
    $ENV{RESVGDIR}/include
    /usr/include
    /usr/local/include
  PATH_SUFFIXES
    resvg
    capi/include
    resvg/capi/include
)

find_library(RESVG_LIBRARIES
  NAMES resvg
  PATHS
    ${RESVGDIR}
    ${RESVGDIR}/lib
    $ENV{RESVGDIR}
    $ENV{RESVGDIR}/lib
    /usr/lib
    /usr/local/lib
  PATH_SUFFIXES
    resvg
    target/release
    resvg/target/release
)

if (RESVG_INCLUDE_DIRS AND RESVG_LIBRARIES)
  set(RESVG_FOUND TRUE)
endif()
set(RESVG_LIBRARIES ${RESVG_LIBRARIES} CACHE STRING "The Resvg library link path")
set(RESVG_INCLUDE_DIRS ${RESVG_INCLUDE_DIRS} CACHE STRING "The Resvg include directories")
set(RESVG_DEFINITIONS "" CACHE STRING "The Resvg CFLAGS")

mark_as_advanced(RESVG_LIBRARIES RESVG_INCLUDE_DIRS RESVG_DEFINITIONS)

# Give a nice error message if some of the required vars are missing.
find_package_handle_standard_args(RESVG
  "Could NOT find RESVG, using Qt SVG parsing instead"
  RESVG_LIBRARIES RESVG_INCLUDE_DIRS )

# Export target
if(RESVG_FOUND AND NOT TARGET RESVG::resvg)
  message(STATUS "Creating IMPORTED target RESVG::resvg")
  if (WIN32)
    # Windows mis-links SHARED library targets
    add_library(RESVG::resvg UNKNOWN IMPORTED)
  else()
    # Linux needs SHARED to link because libresvg has no SONAME
    add_library(RESVG::resvg SHARED IMPORTED)
    set_property(TARGET RESVG::resvg APPEND PROPERTY
      IMPORTED_NO_SONAME TRUE)
  endif()

  set_property(TARGET RESVG::resvg APPEND PROPERTY
    INTERFACE_INCLUDE_DIRECTORIES "${RESVG_INCLUDE_DIRS}")

  set_property(TARGET RESVG::resvg APPEND PROPERTY
    INTERFACE_COMPILE_DEFINITIONS "${RESVG_DEFINITIONS}")

  set_property(TARGET RESVG::resvg APPEND PROPERTY
    IMPORTED_LOCATION "${RESVG_LIBRARIES}")
endif()
