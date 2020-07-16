# vim: ts=2 sw=2
#[=======================================================================[.rst:
FindResvg
---------
Try to find the shared-library build of resvg, the Rust SVG library

IMPORTED targets
^^^^^^^^^^^^^^^^

This module will define up to two :prop_tgt:`IMPORTED` targets if the
appropriate libraries and headers are found:

  ``Resvg::resvg``
  ``Resvg::resvg_static``

Result Variables
^^^^^^^^^^^^^^^^

This module defines the following variables:

::

  Resvg_FOUND         - Library and header files found
  Resvg_INCLUDE_DIRS  - Include directory path
  Resvg_LIBRARIES     - Link path to the library
  Resvg_DEFINITIONS   - Compiler switches (currently unused)

Backwards compatibility
^^^^^^^^^^^^^^^^^^^^^^^

For compatibility with previous versions of this module, uppercase names
for FFmpeg and for all components are also recognized, and all-uppercase
versions of the cache variables are also created.

Control variables
^^^^^^^^^^^^^^^^^

The following variables can be used to provide path hints to the module:

Resvg_ROOT    - (preferred) CMake 3.12+ standard path hint variable
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

find_path(Resvg_INCLUDE_DIRS
  ResvgQt.h
  PATHS
    ${Resvg_ROOT}
    ${Resvg_ROOT}/include
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

foreach(libname resvg libresvg.a)
  find_library(Resvg_${libname}_LIBRARY
    NAMES ${libname}
    PATHS
      ${Resvg_ROOT}
      ${Resvg_ROOT}/lib
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
  list(APPEND Resvg_LIBRARIES ${Resvg_${libname}_LIBRARY})
endforeach()

if (Resvg_INCLUDE_DIRS AND Resvg_LIBRARIES)
  set(Resvg_FOUND TRUE)
endif()
set(Resvg_LIBRARIES ${Resvg_LIBRARIES} CACHE STRING "The Resvg library link path")
set(Resvg_INCLUDE_DIRS ${Resvg_INCLUDE_DIRS} CACHE STRING "The Resvg include directories")
set(Resvg_DEFINITIONS "" CACHE STRING "The Resvg CFLAGS")

mark_as_advanced(Resvg_LIBRARIES Resvg_INCLUDE_DIRS Resvg_DEFINITIONS)

# Give a nice error message if some of the required vars are missing.
find_package_handle_standard_args(Resvg
  "Could NOT find Resvg, using Qt SVG parsing instead"
  Resvg_LIBRARIES Resvg_INCLUDE_DIRS )

# Export shared target
if(${Resvg_resvg_LIBRARY} AND NOT TARGET Resvg::resvg)
  message(STATUS "Creating IMPORTED target Resvg::resvg")
  if (WIN32)
    # Windows mis-links SHARED library targets
    add_library(Resvg::resvg UNKNOWN IMPORTED)
  else()
    # Linux needs SHARED to link because libresvg has no SONAME
    add_library(Resvg::resvg SHARED IMPORTED)
    set_property(TARGET Resvg::resvg APPEND PROPERTY
      IMPORTED_NO_SONAME TRUE)
  endif()

  set_property(TARGET Resvg::resvg APPEND PROPERTY
    INTERFACE_INCLUDE_DIRECTORIES "${Resvg_INCLUDE_DIRS}")

  set_property(TARGET Resvg::resvg APPEND PROPERTY
    INTERFACE_COMPILE_DEFINITIONS "${Resvg_DEFINITIONS}")

  set_property(TARGET Resvg::resvg APPEND PROPERTY
    IMPORTED_LOCATION "${Resvg_resvg_LIBRARY}")
endif()

# Export static-lib target
if(${Resvg_libresvg.a_LIBRARY} AND NOT TARGET Resvg::resvg_static)
  message(STATUS "Creating IMPORTED static target Resvg::resvg_static")
  add_library(Resvg::resvg_static STATIC IMPORTED)

  set_property(TARGET Resvg::resvg_static APPEND PROPERTY
    INTERFACE_INCLUDE_DIRECTORIES "${Resvg_INCLUDE_DIRS}")

  set_property(TARGET Resvg::resvg_static APPEND PROPERTY
    INTERFACE_COMPILE_DEFINITIONS "${Resvg_DEFINITIONS}")

  set_property(TARGET Resvg::resvg_static APPEND PROPERTY
    IMPORTED_LOCATION "${Resvg_libresvg.a_LIBRARY}")
endif()

