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
  CacheDisk
  CacheMemory
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
  QtImageReader
  ReaderBase
  Settings
  Timeline
  # Effects
  ChromaKey
  Crop
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

# Create object library for test executable main(),
# to avoid recompiling for every test
add_library(catch-main OBJECT catch_main.cpp)
target_link_libraries(catch-main PUBLIC Catch2::Catch2)

foreach(tname ${OPENSHOT_TESTS})
  add_executable(openshot-${tname}-test
    ${tname}.cpp
    $<TARGET_OBJECTS:catch-main>
  )
  target_compile_definitions(openshot-${tname}-test PRIVATE
    TEST_MEDIA_PATH="${TEST_MEDIA_PATH}"
  )
  target_link_libraries(openshot-${tname}-test Catch2::Catch2 openshot)

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
