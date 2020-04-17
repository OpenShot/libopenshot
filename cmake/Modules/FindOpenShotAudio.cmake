# - Try to find JUCE-based OpenShot Audio Library
# libopenshot-audio;
# Once done this will define
#  LIBOPENSHOT_AUDIO_FOUND - System has libjuce.so
#  LIBOPENSHOT_AUDIO_INCLUDE_DIRS - The juce.h include directories
#  LIBOPENSHOT_AUDIO_LIBRARIES - The libraries needed to use juce

if("$ENV{LIBOPENSHOT_AUDIO_DIR}" AND NOT "${OpenShotAudio_FIND_QUIETLY}")
    message(STATUS "Looking for OpenShotAudio in: $ENV{LIBOPENSHOT_AUDIO_DIR}")
endif()

# Find the libopenshot-audio header files (check env/cache vars first)
find_path(
  LIBOPENSHOT_AUDIO_INCLUDE_DIR
  JuceHeader.h
  HINTS
    ENV LIBOPENSHOT_AUDIO_DIR
  PATHS
    ${LIBOPENSHOT_AUDIO_DIR}
  PATH_SUFFIXES
    include/libopenshot-audio
    libopenshot-audio
    include
  NO_DEFAULT_PATH
)

# Find the libopenshot-audio header files (fallback to std. paths)
find_path(
  LIBOPENSHOT_AUDIO_INCLUDE_DIR
  JuceHeader.h
  HINTS
    ENV LIBOPENSHOT_AUDIO_DIR
  PATHS
    ${LIBOPENSHOT_AUDIO_DIR}
  PATH_SUFFIXES
    include/libopenshot-audio
    libopenshot-audio
    include
)

# Find libopenshot-audio.so / libopenshot-audio.dll (check env/cache vars first)
find_library(
  LIBOPENSHOT_AUDIO_LIBRARY
  NAMES
    libopenshot-audio
    openshot-audio
  HINTS
    ENV LIBOPENSHOT_AUDIO_DIR
  PATHS
    ${LIBOPENSHOT_AUDIO_DIR}
  PATH_SUFFIXES
    lib/libopenshot-audio
    libopenshot-audio
    lib
  NO_DEFAULT_PATH
)

# Find libopenshot-audio.so / libopenshot-audio.dll (fallback)
find_library(
  LIBOPENSHOT_AUDIO_LIBRARY
  NAMES
    libopenshot-audio
    openshot-audio
  HINTS
    ENV LIBOPENSHOT_AUDIO_DIR
  PATHS
    ${LIBOPENSHOT_AUDIO_DIR}
  PATH_SUFFIXES
    lib/libopenshot-audio
    libopenshot-audio
    lib
)

set(LIBOPENSHOT_AUDIO_LIBRARIES "${LIBOPENSHOT_AUDIO_LIBRARY}")
set(LIBOPENSHOT_AUDIO_LIBRARY "${LIBOPENSHOT_AUDIO_LIBRARIES}")
set(LIBOPENSHOT_AUDIO_INCLUDE_DIRS "${LIBOPENSHOT_AUDIO_INCLUDE_DIR}")

if(LIBOPENSHOT_AUDIO_INCLUDE_DIR AND EXISTS "${LIBOPENSHOT_AUDIO_INCLUDE_DIR}/JuceHeader.h")
  file(STRINGS "${LIBOPENSHOT_AUDIO_INCLUDE_DIR}/JuceHeader.h" libosa_version_str
       REGEX "versionString.*=.*\"[^\"]+\"")
  if(libosa_version_str MATCHES "versionString.*=.*\"([^\"]+)\"")
    set(LIBOPENSHOT_AUDIO_VERSION_STRING ${CMAKE_MATCH_1})
  endif()
  unset(libosa_version_str)
  string(REGEX REPLACE "^([0-9]+\.[0-9]+\.[0-9]+).*$" "\\1"
         LIBOPENSHOT_AUDIO_VERSION "${LIBOPENSHOT_AUDIO_VERSION_STRING}")
endif()

# If we couldn't parse M.N.B version, don't keep any of it
if(NOT LIBOPENSHOT_AUDIO_VERSION)
  unset(LIBOPENSHOT_AUDIO_VERSION)
  unset(LIBOPENSHOT_AUDIO_VERSION_STRING)
endif()

# Determine compatibility with requested version in find_package()
if(OpenShotAudio_FIND_VERSION AND LIBOPENSHOT_AUDIO_VERSION)
  if("${OpenShotAudio_FIND_VERSION}" STREQUAL "${LIBOPENSHOT_AUDIO_VERSION}")
    set(OpenShotAudio_VERSION_EXACT TRUE)
  endif()
  if("${OpenShotAudio_FIND_VERSION}" VERSION_GREATER "${LIBOPENSHOT_AUDIO_VERSION}")
    set(OpenShotAudio_VERSION_COMPATIBLE FALSE)
  else()
    set(OpenShotAudio_VERSION_COMPATIBLE TRUE)
  endif()
endif()

include(FindPackageHandleStandardArgs)
# handle the QUIETLY and REQUIRED arguments and set LIBOPENSHOT_AUDIO_FOUND to TRUE
# if all listed variables are TRUE
find_package_handle_standard_args(OpenShotAudio
  REQUIRED_VARS
    LIBOPENSHOT_AUDIO_LIBRARY
    LIBOPENSHOT_AUDIO_INCLUDE_DIRS
  VERSION_VAR
    LIBOPENSHOT_AUDIO_VERSION_STRING

# Package metadata for FeatureSummary
set_property(GLOBAL PROPERTY
  _CMAKE_OpenShotAudio_DESCRIPTION
   "OpenShot audio library based on JUCE"
)
set_property(GLOBAL PROPERTY
  _CMAKE_OpenShotAudio_URL
  https://github.com/OpenShot/libopenshot-audio
)
