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
  OpenShotAudio_INCLUDE_DIR
  JuceHeader.h
  HINTS
    ENV LIBOPENSHOT_AUDIO_DIR
  PATHS
    ${LIBOPENSHOT_AUDIO_DIR}
    ${OpenShotAudio_ROOT}
    ${OpenShotAudio_INCLUDE_DIR}
  PATH_SUFFIXES
    include/libopenshot-audio
    libopenshot-audio
    include
  NO_DEFAULT_PATH
)

# Find the libopenshot-audio header files (fallback to std. paths)
find_path(
  OpenShotAudio_INCLUDE_DIR
  JuceHeader.h
  HINTS
    ENV LIBOPENSHOT_AUDIO_DIR
  PATHS
    ${LIBOPENSHOT_AUDIO_DIR}
    ${OpenShotAudio_ROOT}
    ${OpenShotAudio_INCLUDE_DIR}
  PATH_SUFFIXES
    include/libopenshot-audio
    libopenshot-audio
    include
)

# Find libopenshot-audio.so / libopenshot-audio.dll (check env/cache vars first)
find_library(
  OpenShotAudio_LIBRARY
  NAMES
    libopenshot-audio
    openshot-audio
  HINTS
    ENV LIBOPENSHOT_AUDIO_DIR
  PATHS
    ${LIBOPENSHOT_AUDIO_DIR}
    ${OpenShotAudio_ROOT}
    ${OpenShotAudio_LIBRARY}
  PATH_SUFFIXES
    lib/libopenshot-audio
    libopenshot-audio
    lib
  NO_DEFAULT_PATH
)

# Find libopenshot-audio.so / libopenshot-audio.dll (fallback)
find_library(
  OpenShotAudio_LIBRARY
  NAMES
    libopenshot-audio
    openshot-audio
  HINTS
    ENV LIBOPENSHOT_AUDIO_DIR
  PATHS
    ${LIBOPENSHOT_AUDIO_DIR}
    ${OpenShotAudio_ROOT}
    ${OpenShotAudio_LIBRARY}
  PATH_SUFFIXES
    lib/libopenshot-audio
    libopenshot-audio
    lib
)

set(OpenShotAudio_LIBRARIES "${OpenShotAudio_LIBRARY}")
set(OpenShotAudio_LIBRARY "${OpenShotAudio_LIBRARIES}")
set(OpenShotAudio_INCLUDE_DIRS "${OpenShotAudio_INCLUDE_DIR}")

if(OpenShotAudio_INCLUDE_DIR AND EXISTS "${OpenShotAudio_INCLUDE_DIR}/JuceHeader.h")
  file(STRINGS "${OpenShotAudio_INCLUDE_DIR}/JuceHeader.h" libosa_version_str
       REGEX "versionString.*=.*\"[^\"]+\"")
  if(libosa_version_str MATCHES "versionString.*=.*\"([^\"]+)\"")
    set(OpenShotAudio_VERSION_STRING ${CMAKE_MATCH_1})
  endif()
  unset(libosa_version_str)
  string(REGEX REPLACE "^([0-9]+\.[0-9]+\.[0-9]+).*$" "\\1"
         OpenShotAudio_VERSION "${OpenShotAudio_VERSION_STRING}")
endif()

# If we couldn't parse M.N.B version, don't keep any of it
if(NOT OpenShotAudio_VERSION)
  unset(OpenShotAudio_VERSION)
  unset(OpenShotAudio_VERSION_STRING)
endif()

# Determine compatibility with requested version in find_package()
if(OpenShotAudio_FIND_VERSION AND OpenShotAudio_VERSION)
  if("${OpenShotAudio_FIND_VERSION}" STREQUAL "${OpenShotAudio_VERSION}")
    set(OpenShotAudio_VERSION_EXACT TRUE)
  endif()
  if("${OpenShotAudio_FIND_VERSION}" VERSION_GREATER "${OpenShotAudio_VERSION}")
    set(OpenShotAudio_VERSION_COMPATIBLE FALSE)
  else()
    set(OpenShotAudio_VERSION_COMPATIBLE TRUE)
  endif()
endif()

include(FindPackageHandleStandardArgs)
# handle the QUIETLY and REQUIRED arguments and set OpenShotAudio_FOUND to TRUE
# if all listed variables are TRUE
find_package_handle_standard_args(OpenShotAudio
  REQUIRED_VARS
    OpenShotAudio_LIBRARIES
    OpenShotAudio_INCLUDE_DIRS
  VERSION_VAR
    OpenShotAudio_VERSION_STRING
)

if(OpenShotAudio_FOUND)
  set(OpenShotAudio_INCLUDE_DIRS "${OpenShotAudio_INCLUDE_DIRS}"
    CACHE PATH "The paths to libopenshot-audio's header files" FORCE)
  set(OpenShotAudio_LIBRARIES "${OpenShotAudio_LIBRARIES}"
    CACHE STRING "The libopenshot-audio library to link with" FORCE)
  if(DEFINED OpenShotAudio_VERSION)
    set(OpenShotAudio_VERSION ${OpenShotAudio_VERSION}
      CACHE STRING "The version of libopenshot-audio detected" FORCE)
  endif()
endif()

if(OpenShotAudio_FOUND AND NOT TARGET OpenShot::Audio)
  message(STATUS "Creating IMPORTED target OpenShot::Audio")
  if(WIN32)
    add_library(OpenShot::Audio UNKNOWN IMPORTED)
  else()
    add_library(OpenShot::Audio SHARED IMPORTED)
  endif()

  set_property(TARGET OpenShot::Audio APPEND PROPERTY
    INTERFACE_INCLUDE_DIRECTORIES "${OpenShotAudio_INCLUDE_DIRS}")

  # Juce requires either DEBUG or NDEBUG to be defined on MacOS.
  # -DNDEBUG is set by cmake for all release configs, so add
  # -DDEBUG for debug builds. We'll do this for all OSes, even
  # though only MacOS requires it.
  # The generator expression translates to:
  #   CONFIG == "DEBUG" ? "DEBUG" : ""
  set_property(TARGET OpenShot::Audio APPEND PROPERTY
    INTERFACE_COMPILE_DEFINITIONS $<$<CONFIG:DEBUG>:DEBUG>)

  # For the Ruby bindings
  set_property(TARGET OpenShot::Audio APPEND PROPERTY
    INTERFACE_COMPILE_DEFINITIONS HAVE_ISFINITE=1)

  if(WIN32)
    set_property(TARGET OpenShot::Audio APPEND PROPERTY
      INTERFACE_COMPILE_DEFINITIONS IGNORE_JUCE_HYPOT=1)
    set_property(TARGET OpenShot::Audio APPEND PROPERTY
      INTERFACE_COMPILE_OPTIONS -include cmath)
  elseif(APPLE)
    # Prevent compiling with __cxx11
    set_property(TARGET OpenShot::Audio APPEND PROPERTY
      INTERFACE_COMPILE_DEFINITIONS _GLIBCXX_USE_CXX11_ABI=0)
    list(APPEND framework_deps
      "-framework Carbon"
      "-framework Cocoa"
      "-framework CoreFoundation"
      "-framework CoreAudio"
      "-framework CoreMidi"
      "-framework IOKit"
      "-framework AGL"
      "-framework AudioToolbox"
      "-framework QuartzCore"
      "-lobjc"
      "-framework Accelerate"
      )
    target_link_libraries(OpenShot::Audio INTERFACE ${framework_deps})
  endif()

  set_property(TARGET OpenShot::Audio APPEND PROPERTY
    IMPORTED_LOCATION "${OpenShotAudio_LIBRARIES}")
endif()
