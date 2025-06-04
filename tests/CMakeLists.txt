##################### tests/CMakeLists.txt (libopenshot) ######################
# @brief CMake build file for libopenshot (used to generate makefiles)
# @author Jonathan Thomas <jonathan@openshot.org>
#
# @section LICENSE
#
# Copyright (c) 2008-2019 OpenShot Studios, LLC
#
# SPDX-License-Identifier: LGPL-3.0-or-later

# Allow spaces in test names
if(POLICY CMP0110)
  cmake_policy(SET CMP0110 NEW)
endif()

# Test media path, used by unit tests for input data
file(TO_NATIVE_PATH "${PROJECT_SOURCE_DIR}/examples/" TEST_MEDIA_PATH)

###
###  TEST SOURCE FILES
###
set(OPENSHOT_TESTS
  AudioDeviceManager
  AudioWaveformer
  CacheDisk
  CacheMemory
  Caption
  Clip
  Color
  Coordinate
  DummyReader
  FFmpegReader
  FFmpegWriter
  Fraction
  Frame
  FrameMapper
  KeyFrame
  Point
  Profiles
  QtImageReader
  ReaderBase
  Settings
  SphericalMetadata
  Timeline
  VideoCacheThread
  # Effects
  ColorMap
  ChromaKey
  Crop
  LensFlare
  Sharpen
  SphericalEffect
)

# ImageMagick related test files
if($CACHE{HAVE_IMAGEMAGICK})
  list(APPEND OPENSHOT_TESTS
    ImageWriter
  )
endif()

# OPENCV RELATED TEST FILES
if($CACHE{HAVE_OPENCV})
  list(APPEND OPENSHOT_TESTS
    CVTracker
    CVStabilizer
    CVOutline
    # CVObjectDetection
  )
endif()

###
### Catch2 unit tests
###
include(CTest)
include(Catch)

if(NOT BUILD_TESTING)
  return()
endif()

add_library(openshot_catch2 INTERFACE)
target_include_directories(openshot_catch2 INTERFACE
  "${CMAKE_CURRENT_BINARY_DIR}"
)
target_compile_definitions(openshot_catch2 INTERFACE
  TEST_MEDIA_PATH="${TEST_MEDIA_PATH}")

if(TARGET Catch2::Catch2WithMain)
  # Catch2 v3 works a bit differently
  configure_file(catch2v3.h.in openshot_catch.h)
  target_link_libraries(openshot_catch2 INTERFACE Catch2::Catch2WithMain)
else()
  configure_file(catch2v2.h.in openshot_catch.h)
  # Create object library for test executable main(),
  # to avoid recompiling for every test
  add_library(catch-main OBJECT catch_main.cpp)
  target_link_libraries(catch-main PUBLIC Catch2::Catch2)
  target_include_directories(catch-main PUBLIC
    "${CMAKE_CURRENT_BINARY_DIR}")
  target_link_libraries(openshot_catch2 INTERFACE Catch2::Catch2)
  target_sources(openshot_catch2 INTERFACE $<TARGET_OBJECTS:catch-main>)
endif()

foreach(tname ${OPENSHOT_TESTS})
  add_executable(openshot-${tname}-test
    ${tname}.cpp
  )
  target_link_libraries(openshot-${tname}-test
    openshot_catch2
    openshot
  )

  # Automatically configure CTest targets from Catch2 test cases
  catch_discover_tests(
    openshot-${tname}-test
    TEST_PREFIX ${tname}:
    PROPERTIES
      LABELS ${tname}
  )
  list(APPEND CATCH2_TEST_TARGETS openshot-${tname}-test)
  list(APPEND CATCH2_TEST_NAMES ${tname})
endforeach()

# Add an additional special-case test, for an envvar-dependent setting
catch_discover_tests(
  openshot-Settings-test
  TEST_LIST Settings_EXTRA_TESTS
  TEST_SPEC "[environment]"
  TEST_PREFIX Settings:
  TEST_SUFFIX "(enabled)"
  TEST_WORKING_DIR "${_test_dir}"
  PROPERTIES
    LABELS Settings
    ENVIRONMENT "LIBOPENSHOT_DEBUG=1"
)

# Export target list for coverage use
set(UNIT_TEST_TARGETS ${CATCH2_TEST_TARGETS} PARENT_SCOPE)
set(UNIT_TEST_NAMES ${CATCH2_TEST_NAMES} PARENT_SCOPE)
