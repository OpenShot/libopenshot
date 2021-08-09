# SPDX-License-Identifier: BSD-3-Clause

# vim: ts=2 sw=2
#[=======================================================================[.rst:
FindFFmpeg
----------
Try to find the requested ffmpeg components(default: avformat, avutil, avcodec)

IMPORTED targets
^^^^^^^^^^^^^^^^

This module defines :prop_tgt:`IMPORTED` targets ``FFmpeg:<component>`` for
each found component (see below).

Components
^^^^^^^^^^

The module recognizes the following components:

::

  avcodec     - target FFmpeg::avcodec
  avdevice    - target FFmpeg::avdevice
  avformat    - target FFmpeg::avformat
  avfilter    - target FFmpeg::avfilter
  avutil      - target FFmpeg::avutil
  postproc    - target FFmpeg::postproc
  swscale     - target FFmpeg::swscale
  swresample  - target FFmpeg::swresample
  avresample  - target FFmpeg::avresample

Result Variables
^^^^^^^^^^^^^^^^

This module defines the following variables:

::

  FFmpeg_FOUND         - System has the all required components.
  FFmpeg_INCLUDE_DIRS  - Include directory necessary for using the required components headers.
  FFmpeg_LIBRARIES     - Link these to use the required ffmpeg components.
  FFmpeg_DEFINITIONS   - Compiler switches required for using the required ffmpeg components.
  FFmpeg_VERSION       - The FFmpeg package version found.

For each component, ``FFmpeg_<component>_FOUND`` will be set if the component is available.

For each ``FFmpeg_<component>_FOUND``,  the following variables will be defined:

::

  FFmpeg_<component>_INCLUDE_DIRS - Include directory necessary for using the
                                    <component> headers
  FFmpeg_<component>_LIBRARIES    - Link these to use <component>
  FFmpeg_<component>_DEFINITIONS  - Compiler switches required for <component>
  FFmpeg_<component>_VERSION      - The components version

Backwards compatibility
^^^^^^^^^^^^^^^^^^^^^^^

For compatibility with previous versions of this module, uppercase names
for FFmpeg and for all components are also recognized, and all-uppercase
versions of the cache variables are also created.

Revision history
^^^^^^^^^^^^^^^^
ca. 2019   - Create CMake targets for discovered components
2019-06-25 - No longer probe for non-requested components
           - Added fallback version.h parsing for components, when
             pkgconfig is missing
           - Added parsing of libavutil/ffversion.h for FFmpeg_VERSION
           - Adopt standard FFmpeg_<component>_<property> variable names
           - Switch to full signature for FPHSA, use HANDLE_COMPONENTS

Copyright (c) 2006, Matthias Kretz, <kretz@kde.org>
Copyright (c) 2008, Alexander Neundorf, <neundorf@kde.org>
Copyright (c) 2011, Michael Jansen, <kde@michael-jansen.biz>
Copyright (c) 2019-2021, FeRD (Frank Dana) <ferdnyc@gmail.com>

Redistribution and use is allowed according to the terms of the BSD license.
For details see the accompanying COPYING-CMAKE-SCRIPTS file.
#]=======================================================================]
include(FindPackageHandleStandardArgs)

set(FFmpeg_ALL_COMPONENTS avcodec avdevice avformat avfilter avutil postproc swscale swresample avresample)

# Default to all components, if not specified
if (FFMPEG_FIND_COMPONENTS AND NOT FFmpeg_FIND_COMPONENTS)
  set(FFmpeg_FIND_COMPONENTS ${FFMPEG_FIND_COMPONENTS})
endif ()
if (NOT FFmpeg_FIND_COMPONENTS)
  set(FFmpeg_FIND_COMPONENTS ${FFmpeg_ALL_COMPONENTS})
endif ()


#
### Macro: set_component_found
#
# Marks the given component as found if both *_LIBRARIES AND *_INCLUDE_DIRS is present.
#
macro(set_component_found _component )
  if (FFmpeg_${_component}_LIBRARIES AND FFmpeg_${_component}_INCLUDE_DIRS)
    # message(STATUS "FFmpeg - ${_component} found.")
    set(FFmpeg_${_component}_FOUND TRUE)
  #else ()
  #  if (NOT FFmpeg_FIND_QUIETLY AND NOT FFMPEG_FIND_QUIETLY)
  #    message(STATUS "FFmpeg - ${_component} not found.")
  #  endif ()
  endif ()
endmacro()

#
### Macro: parse_lib_version
#
# Reads the file '${_pkgconfig}/version.h' in the component's _INCLUDE_DIR,
# and parses #define statements for COMPONENT_VERSION_(MAJOR|MINOR|PATCH)
# into a dotted string ${_component}_VERSION.
#
# Needed if the version is not supplied via pkgconfig's PC_${_component}_VERSION
macro(parse_lib_version _component _libname )
  set(_version_h "${FFmpeg_${_component}_INCLUDE_DIRS}/${_libname}/version.h")
  if(EXISTS "${_version_h}")
    #message(STATUS "Parsing ${_component} version from ${_version_h}")
    string(TOUPPER "${_libname}" _prefix)
    set(_parts)
    foreach(_lvl MAJOR MINOR MICRO)
      file(STRINGS "${_version_h}" _lvl_version
      REGEX "^[ \t]*#define[ \t]+${_prefix}_VERSION_${_lvl}[ \t]+[0-9]+[ \t]*$")
      string(REGEX REPLACE
        "^.*${_prefix}_VERSION_${_lvl}[ \t]+([0-9]+)[ \t]*$"
        "\\1"
        _lvl_match "${_lvl_version}")
      list(APPEND _parts "${_lvl_match}")
    endforeach()
    list(JOIN _parts "." FFmpeg_${_component}_VERSION)
    message(STATUS "Found ${_component} version: ${FFmpeg_${_component}_VERSION}")
  endif()
endmacro()

#
### Macro: find_component
#
# Checks for the given component by invoking pkgconfig and then looking up the libraries and
# include directories.
#
macro(find_component _component _pkgconfig _library _header)

  if (NOT WIN32)
    # use pkg-config to get the directories and then use these values
    # in the FIND_PATH() and FIND_LIBRARY() calls
    find_package(PkgConfig)
    if (PKG_CONFIG_FOUND)
      pkg_check_modules(PC_${_component} ${_pkgconfig})
    endif ()
  endif()

  find_path(FFmpeg_${_component}_INCLUDE_DIRS ${_header}
    HINTS
    /opt/
    /opt/include/
    ${PC_${_component}_INCLUDEDIR}
    ${PC_${_component}_INCLUDE_DIRS}
    $ENV{FFMPEGDIR}/include/
    $ENV{FFMPEGDIR}/include/ffmpeg/
    PATH_SUFFIXES
    ffmpeg
    )

  find_library(FFmpeg_${_component}_LIBRARIES NAMES ${_library}
    HINTS
    ${PC_${_component}_LIBDIR}
    ${PC_${_component}_LIBRARY_DIRS}
    $ENV{FFMPEGDIR}/lib/
    $ENV{FFMPEGDIR}/lib/ffmpeg/
    $ENV{FFMPEGDIR}/bin/
    )

  set(FFmpeg_${_component}_DEFINITIONS  ${PC_${_component}_CFLAGS_OTHER} CACHE STRING "The ${_component} CFLAGS.")

  # Take version from PkgConfig, or parse from its version.h header
  if (PC_${_component}_VERSION)
    set(FFmpeg_${_component}_VERSION ${PC_${_component}_VERSION})
  else()
    parse_lib_version(${_component} ${_pkgconfig})
  endif()

  set(FFmpeg_${_component}_VERSION ${FFmpeg_${_component}_VERSION} CACHE STRING "The ${_component} version number.")

  set_component_found(${_component})

  mark_as_advanced(
    FFmpeg_${_component}_INCLUDE_DIRS
    FFmpeg_${_component}_LIBRARIES
    FFmpeg_${_component}_DEFINITIONS
    FFmpeg_${_component}_VERSION
    )

endmacro()

#
### Macro: parse_ff_version
#
# Read the libavutil/ffversion.h file and extract the definition
# for FFMPEG_VERSION, to use as our version string.
macro (parse_ff_version)
  set(_header "${FFmpeg_avutil_INCLUDE_DIRS}/libavutil/ffversion.h")
  if(EXISTS "${_header}")
    #message(STATUS "Parsing ffmpeg version from ${_header}")
    file(STRINGS "${_header}" _version_def
    REGEX "^#define[ \t]+FFMPEG_VERSION[ \t]+\".*\"[ \t]*$")
    string(REGEX REPLACE
      "^.*FFMPEG_VERSION[ \t]+\"(.*)\".*$"
      "\\1"
      FFmpeg_VERSION "${_version_def}")
    #message(STATUS "Found FFmpeg version: ${FFmpeg_VERSION}")
  endif()
endmacro()

# Configs for all possible component.
set(avcodec_params    libavcodec    avcodec  libavcodec/avcodec.h)
set(avdevice_params   libavdevice   avdevice libavdevice/avdevice.h)
set(avformat_params   libavformat   avformat libavformat/avformat.h)
set(avfilter_params   libavfilter   avfilter libavfilter/avfilter.h)
set(avutil_params     libavutil     avutil   libavutil/avutil.h)
set(postproc_params   libpostproc   postproc libpostproc/postprocess.h)
set(swscale_params    libswscale    swscale  libswscale/swscale.h)
set(swresample_params libswresample swresample libswresample/swresample.h)
set(avresample_params libavresample avresample libavresample/avresample.h)

# Gather configs for each requested component
foreach(_component ${FFmpeg_FIND_COMPONENTS})
  string(TOLOWER "${_component}" _component)
  # Only probe if not already _FOUND (expensive)
  if (NOT FFmpeg_${_component}_FOUND)
    find_component(${_component} ${${_component}_params})
  endif()

  # Add the component's configs to the FFmpeg_* variables
  if (FFmpeg_${_component}_FOUND)
    # message(STATUS "Requested component ${_component} present.")
    set(FFmpeg_LIBRARIES   ${FFmpeg_LIBRARIES}   ${FFmpeg_${_component}_LIBRARIES})
    set(FFmpeg_DEFINITIONS ${FFmpeg_DEFINITIONS} ${FFmpeg_${_component}_DEFINITIONS})
    list(APPEND FFmpeg_INCLUDE_DIRS ${FFmpeg_${_component}_INCLUDE_DIRS})
  else ()
    # message(STATUS "Requested component ${_component} missing.")
  endif ()
endforeach()

# Make sure we've probed for avutil
if (NOT FFmpeg_avutil_FOUND)
  find_component(avutil libavutil avutil libavutil/avutil.h)
endif()
# Get the overall FFmpeg version from libavutil/ffversion.h
parse_ff_version()

# Build the result lists with duplicates removed, in case of repeated
# invocations or component redundancy.
if (FFmpeg_INCLUDE_DIRS)
  list(REMOVE_DUPLICATES FFmpeg_INCLUDE_DIRS)
endif()
if (FFmpeg_LIBRARIES)
  list(REMOVE_DUPLICATES FFmpeg_LIBRARIES)
endif()
if(FFmpeg_DEFINITIONS)
  list(REMOVE_DUPLICATES FFmpeg_DEFINITIONS)
endif ()

# cache the vars.
set(FFmpeg_INCLUDE_DIRS ${FFmpeg_INCLUDE_DIRS} CACHE STRING "The FFmpeg include directories." FORCE)
set(FFmpeg_LIBRARIES    ${FFmpeg_LIBRARIES}    CACHE STRING "The FFmpeg libraries." FORCE)
set(FFmpeg_DEFINITIONS  ${FFmpeg_DEFINITIONS}  CACHE STRING "The FFmpeg cflags." FORCE)
set(FFmpeg_VERSION      ${FFmpeg_VERSION}      CACHE STRING "The overall FFmpeg version.")

mark_as_advanced(
  FFmpeg_INCLUDE_DIRS
  FFmpeg_LIBRARIES
  FFmpeg_DEFINITIONS
  FFmpeg_VERSION
)

# Backwards compatibility
foreach(_suffix INCLUDE_DIRS LIBRARIES DEFINITIONS VERSION)
  get_property(_help CACHE FFmpeg_${_suffix} PROPERTY HELPSTRING)
  set(FFMPEG_${_suffix} ${FFmpeg_${_suffix}} CACHE STRING "${_help}" FORCE)
  mark_as_advanced(FFMPEG_${_suffix})
endforeach()
foreach(_component ${FFmpeg_ALL_COMPONENTS})
  if(FFmpeg_${_component}_FOUND)
    string(TOUPPER "${_component}" _uc_component)
    set(FFMPEG_${_uc_component}_FOUND TRUE)
    foreach(_suffix INCLUDE_DIRS LIBRARIES DEFINITIONS VERSION)
      get_property(_help CACHE FFmpeg_${_component}_${_suffix} PROPERTY HELPSTRING)
      set(FFMPEG_${_uc_component}_${_suffix} ${FFmpeg_${_component}_${_suffix}} CACHE STRING "${_help}" FORCE)
      mark_as_advanced(FFMPEG_${_uc_component}_${_suffix})
    endforeach()
  endif()
endforeach()

# Compile the list of required vars
set(_FFmpeg_REQUIRED_VARS FFmpeg_LIBRARIES FFmpeg_INCLUDE_DIRS)
# XXX: HANDLE_COMPONENTS should take care of this, maybe? -FeRD
# foreach (_component ${FFmpeg_FIND_COMPONENTS})
#   list(APPEND _FFmpeg_REQUIRED_VARS
#     FFmpeg_${_component}_LIBRARIES
#     FFmpeg_${_component}_INCLUDE_DIRS)
# endforeach ()

# Give a nice error message if some of the required vars are missing.
find_package_handle_standard_args(FFmpeg
  REQUIRED_VARS ${_FFmpeg_REQUIRED_VARS}
  VERSION_VAR FFmpeg_VERSION
  HANDLE_COMPONENTS
)

# Export targets for each found component
foreach (_component ${FFmpeg_FIND_COMPONENTS})

  if(FFmpeg_${_component}_FOUND)
    #message(STATUS "Creating IMPORTED target FFmpeg::${_component}")

    if(NOT TARGET FFmpeg::${_component})
      add_library(FFmpeg::${_component} UNKNOWN IMPORTED)

      set_target_properties(FFmpeg::${_component} PROPERTIES
        INTERFACE_INCLUDE_DIRECTORIES
          "${FFmpeg_${_component}_INCLUDE_DIRS}")

      set_property(TARGET FFmpeg::${_component} APPEND PROPERTY
        INTERFACE_COMPILE_DEFINITIONS
          "${FFmpeg_${_component}_DEFINITIONS}")

      set_property(TARGET FFmpeg::${_component} APPEND PROPERTY
        IMPORTED_LOCATION "${FFmpeg_${_component}_LIBRARIES}")
    endif()

  endif()

endforeach()
