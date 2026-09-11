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

# Benchmark executable
add_executable(openshot-benchmark Benchmark.cpp BenchmarkOptions.cpp)
target_compile_definitions(openshot-benchmark PRIVATE -DTEST_MEDIA_PATH="${TEST_MEDIA_PATH}")
target_link_libraries(openshot-benchmark openshot)

###
###  TEST SOURCE FILES
###
set(OPENSHOT_TESTS
  AudioDeviceManager
  AudioRecorder
  AnimatedCurve
  AudioWaveformer
  CameraCaptureReader
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
  FrameScope
  FrameMapper
  KeyFrame
  Point
  Profiles
  QtPlayer
  QtImageReader
  ReaderBase
  NativeArm64ProcessOracle
  Settings
  ScreenCaptureReader
  SphericalMetadata
  Timeline
  VideoCacheThread
  # Effects
  AudioVisualization
  Bars
  BeatSync
  Blur
  ColorGrade
  ColorMap
  ChromaKey
  Crop
  Displace
  FilmGrain
  Glow
  LensFlare
  AnalogTape
  BenchmarkArgs
  EffectMask
  Mask
  Pixelate
  Saturation
  Sharpen
  Shadow
  SphericalEffect
  Timer
  DenoiseImage
  WaveEffect
)

# ImageMagick related test files
if($CACHE{HAVE_IMAGEMAGICK})
  list(APPEND OPENSHOT_TESTS
    ImageReader
    ImageWriter
  )
endif()

# OPENCV RELATED TEST FILES
if($CACHE{HAVE_OPENCV})
  list(APPEND OPENSHOT_TESTS
    CVTracker
    CVStabilizer
    CVOutline
    ObjectMask
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
  set(test_sources
    ${tname}.cpp
  )
  if(tname STREQUAL "BenchmarkArgs")
    list(APPEND test_sources BenchmarkOptions.cpp)
  endif()
  add_executable(openshot-${tname}-test
    ${test_sources}
  )
  target_link_libraries(openshot-${tname}-test
    openshot_catch2
    openshot
  )

  set(test_properties
    LABELS ${tname}
  )
  if(tname STREQUAL "FFmpegReader")
    # Catch2 v2 has no native SKIP assertion. Report unavailable VAAPI
    # prerequisites as skipped in CTest rather than as a passing decode test.
    list(APPEND test_properties
      SKIP_REGULAR_EXPRESSION "Skipping VAAPI test:"
    )
  endif()
  if(tname STREQUAL "Caption" OR tname STREQUAL "Timer")
    list(APPEND test_properties
      ENVIRONMENT "QT_QPA_PLATFORM=minimal"
    )
  endif()

  # Automatically configure CTest targets from Catch2 test cases
  catch_discover_tests(
    openshot-${tname}-test
    TEST_PREFIX ${tname}:
    PROPERTIES
      ${test_properties}
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
