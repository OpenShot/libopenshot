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

  FFMPEG_FOUND         - System has the all required components.
  FFMPEG_INCLUDE_DIRS  - Include directory necessary for using the required components headers.
  FFMPEG_LIBRARIES     - Link these to use the required ffmpeg components.
  FFMPEG_DEFINITIONS   - Compiler switches required for using the required ffmpeg components.

For each component, ``<component>_FOUND`` will be set if the component is available.
 
For each ``<component>_FOUND``,  the following variables will be defined:

::

  <component>_INCLUDE_DIRS - Include directory necessary for using the <component> headers
  <component>_LIBRARIES    - Link these to use <component>
  <component>_DEFINITIONS  - Compiler switches required for using <component>
  <component>_VERSION      - The components version

Backwards compatibility
^^^^^^^^^^^^^^^^^^^^^^^

For compatibility with previous versions of this module, uppercase names
for FFmpeg and for all components are also recognized, and all-uppercase
versions of the cache variables are also created.

Copyright (c) 2006, Matthias Kretz, <kretz@kde.org>
Copyright (c) 2008, Alexander Neundorf, <neundorf@kde.org>
Copyright (c) 2011, Michael Jansen, <kde@michael-jansen.biz>
Copyright (c) 2019, FeRD (Frank Dana) <ferdnyc@gmail.com>

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
  if (${_component}_LIBRARIES AND ${_component}_INCLUDE_DIRS)
    # message(STATUS "FFmpeg - ${_component} found.")
    set(${_component}_FOUND TRUE)
  else ()
    if (NOT FFmpeg_FIND_QUIETLY AND NOT FFMPEG_FIND_QUIETLY)
      message(STATUS "FFmpeg - ${_component} not found.")
    endif ()
  endif ()
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
  endif (NOT WIN32)

  find_path(${_component}_INCLUDE_DIRS ${_header}
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

  find_library(${_component}_LIBRARIES NAMES ${_library}
    HINTS
    ${PC_${_component}_LIBDIR}
    ${PC_${_component}_LIBRARY_DIRS}
    $ENV{FFMPEGDIR}/lib/
    $ENV{FFMPEGDIR}/lib/ffmpeg/
    $ENV{FFMPEGDIR}/bin/
    )

  set(${_component}_DEFINITIONS  ${PC_${_component}_CFLAGS_OTHER} CACHE STRING "The ${_component} CFLAGS.")
  set(${_component}_VERSION      ${PC_${_component}_VERSION}      CACHE STRING "The ${_component} version number.")

  set_component_found(${_component})

  mark_as_advanced(
    ${_component}_INCLUDE_DIRS
    ${_component}_LIBRARIES
    ${_component}_DEFINITIONS
    ${_component}_VERSION
    )

endmacro()


# Check for cached results. If there are skip the costly part.
if (NOT FFmpeg_LIBRARIES)

  # Check for all possible component.
  find_component(avcodec    libavcodec    avcodec  libavcodec/avcodec.h)
  find_component(avdevice   libavdevice   avdevice libavdevice/avdevice.h)
  find_component(avformat   libavformat   avformat libavformat/avformat.h)
  find_component(avfilter   libavfilter   avfilter libavfilter/avfilter.h)
  find_component(avutil     libavutil     avutil   libavutil/avutil.h)
  find_component(postproc   libpostproc   postproc libpostproc/postprocess.h)
  find_component(swscale    libswscale    swscale  libswscale/swscale.h)
  find_component(swresample libswresample swresample libswresample/swresample.h)
  find_component(avresample libavresample avresample libavresample/avresample.h)
else()
  # Just set the noncached _FOUND vars for the components.
  foreach(_component ${FFmpeg_ALL_COMPONENTS})
    set_component_found(${_component})
  endforeach ()
endif()

# Check if the requested components were found and add their stuff to the FFmpeg_* vars.
foreach (_component ${FFmpeg_FIND_COMPONENTS})
  string(TOLOWER "${_component}" _component)
  if (${_component}_FOUND)
    # message(STATUS "Requested component ${_component} present.")
    set(FFmpeg_LIBRARIES   ${FFmpeg_LIBRARIES}   ${${_component}_LIBRARIES})
    set(FFmpeg_DEFINITIONS ${FFmpeg_DEFINITIONS} ${${_component}_DEFINITIONS})
    list(APPEND FFmpeg_INCLUDE_DIRS ${${_component}_INCLUDE_DIRS})
  else ()
    # message(STATUS "Requested component ${_component} missing.")
  endif ()
endforeach ()

# Build the result lists with duplicates removed, in case of repeated
# invocations.
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

mark_as_advanced(FFmpeg_INCLUDE_DIRS
  FFmpeg_LIBRARIES
  FFmpeg_DEFINITIONS)

# Backwards compatibility
foreach(_suffix INCLUDE_DIRS LIBRARIES DEFINITIONS)
  get_property(_help CACHE FFmpeg_${_suffix} PROPERTY HELPSTRING)
  set(FFMPEG_${_suffix} ${FFmpeg_${_suffix}} CACHE STRING "${_help}" FORCE)
  mark_as_advanced(FFMPEG_${_suffix})
endforeach()
foreach(_component ${FFmpeg_ALL_COMPONENTS})
  if(${_component}_FOUND)
    string(TOUPPER "${_component}" _uc_component)
    set(${_uc_component}_FOUND TRUE)
    foreach(_suffix INCLUDE_DIRS LIBRARIES DEFINITIONS VERSION)
      get_property(_help CACHE ${_component}_${_suffix} PROPERTY HELPSTRING)
      set(${_uc_component}_${_suffix} ${${_component}_${_suffix}} CACHE STRING "${_help}" FORCE)
      mark_as_advanced(${_uc_component}_${_suffix})
    endforeach()
  endif()
endforeach()

# Compile the list of required vars
set(_FFmpeg_REQUIRED_VARS FFmpeg_LIBRARIES FFmpeg_INCLUDE_DIRS)
foreach (_component ${FFmpeg_FIND_COMPONENTS})
  list(APPEND _FFmpeg_REQUIRED_VARS
    ${_component}_LIBRARIES
    ${_component}_INCLUDE_DIRS)
endforeach ()

# Give a nice error message if some of the required vars are missing.
find_package_handle_standard_args(FFmpeg DEFAULT_MSG ${_FFmpeg_REQUIRED_VARS})

# Export targets for each found component
foreach (_component ${FFmpeg_ALL_COMPONENTS})

  if(${_component}_FOUND)
    # message(STATUS "Creating IMPORTED target FFmpeg::${_component}")

    if(NOT TARGET FFmpeg::${_component})
      add_library(FFmpeg::${_component} UNKNOWN IMPORTED)

      set_target_properties(FFmpeg::${_component} PROPERTIES
        INTERFACE_INCLUDE_DIRECTORIES "${${_component}_INCLUDE_DIRS}")

      set_property(TARGET FFmpeg::${_component} APPEND PROPERTY
        INTERFACE_COMPILE_DEFINITIONS "${${_component}_DEFINITIONS}")

      set_property(TARGET FFmpeg::${_component} APPEND PROPERTY
        IMPORTED_LOCATION "${${_component}_LIBRARIES}")
    endif()

  endif()

endforeach()
