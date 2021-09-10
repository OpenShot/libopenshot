####################### CMakeLists.txt (libopenshot) #########################
# @brief CMake build file for libopenshot (used to generate makefiles)
# @author Jonathan Thomas <jonathan@openshot.org>
#
# @section LICENSE
#
# Copyright (c) 2008-2019 OpenShot Studios, LLC
# <http://www.openshotstudios.com/>. This file is part of
# OpenShot Library (libopenshot), an open-source project dedicated to
# delivering high quality video editing and animation solutions to the
# world. For more information visit <http://www.openshot.org/>.
#
# OpenShot Library (libopenshot) is free software: you can redistribute it
# and/or modify it under the terms of the GNU Lesser General Public License
# as published by the Free Software Foundation, either version 3 of the
# License, or (at your option) any later version.
#
# OpenShot Library (libopenshot) is distributed in the hope that it will be
# useful, but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
# GNU Lesser General Public License for more details.
#
# You should have received a copy of the GNU Lesser General Public License
# along with OpenShot Library. If not, see <http://www.gnu.org/licenses/>.
################################################################################

cmake_minimum_required(VERSION 3.10...3.20 FATAL_ERROR)

message("\
-----------------------------------------------------------------
          Welcome to the OpenShot Build System!

CMake will now check libopenshot's build dependencies and inform
you of any missing files or other issues.

For more information, please visit <http://www.openshot.org/>.
-----------------------------------------------------------------")

################ ADD CMAKE MODULES ##################
set(CMAKE_MODULE_PATH "${CMAKE_SOURCE_DIR}/cmake/Modules")

################ PROJECT VERSION ####################
set(PROJECT_VERSION_FULL "0.2.7")
set(PROJECT_SO_VERSION 21)

# Remove the dash and anything following, to get the #.#.# version for project()
STRING(REGEX REPLACE "\-.*$" "" VERSION_NUM "${PROJECT_VERSION_FULL}")

################### SETUP PROJECT ###################
# This will define the following variables
# PROJECT_NAME
# PROJECT_VERSION, libopenshot_VERSION
# PROJECT_VERSION_MAJOR, libopenshot_VERSION_MAJOR
# PROJECT_VERSION_MINOR, libopenshot_VERSION_MINOR
# PROJECT_VERSION_PATCH, libopenshot_VERSION_PATCH
PROJECT(libopenshot LANGUAGES C CXX VERSION ${VERSION_NUM})

message("
Generating build files for OpenShot with CMake ${CMAKE_VERSION}
  Building ${PROJECT_NAME} (version ${PROJECT_VERSION})
  SO/API/ABI Version: ${PROJECT_SO_VERSION}
")

# Define install paths according to system conventions
# XXX: This must be AFTER THE PROJECT() COMMAND w/ languages enabled,
#      in order to properly configure CMAKE_INSTALL_LIBDIR path
include(GNUInstallDirs)

# Collect and display summary of options/dependencies
include(FeatureSummary)

# Unit test management
include(CTest)

################ OPTIONS ##################
# Optional build settings for libopenshot
option(ENABLE_IWYU "Enable 'Include What You Use' scanner (CMake 3.3+)" OFF)

option(ENABLE_PARALLEL_CTEST "Run CTest using multiple processors" ON)
option(VERBOSE_TESTS "Run CTest with maximum verbosity" OFF)
option(ENABLE_COVERAGE "Scan test coverage using gcov and report" OFF)

option(ENABLE_DOCS "Build API documentation (requires Doxygen)" ON)

option(APPIMAGE_BUILD "Build to install in an AppImage (Linux only)" OFF)

option(USE_SYSTEM_JSONCPP "Use system installed JsonCpp, if found" ON)
option(DISABLE_BUNDLED_JSONCPP "Don't fall back to bundled JsonCpp" OFF)
option(ENABLE_MAGICK "Use ImageMagick, if available" ON)
option(ENABLE_OPENCV "Build with OpenCV algorithms (requires Boost, Protobuf 3)" ON)
option(USE_HW_ACCEL "Enable hardware-accelerated encoding-decoding with FFmpeg 3.4+" ON)

# Legacy commandline override
if (DISABLE_TESTS)
  set(BUILD_TESTING OFF)
endif()

if(ENABLE_TESTS)
  set(BUILD_TESTING ${ENABLE_TESTS})
endif()

#### Work around a GCC < 9 bug with handling of _Pragma() in macros
#### See https://gcc.gnu.org/bugzilla/show_bug.cgi?id=55578
if ((${CMAKE_CXX_COMPILER_ID} STREQUAL "GNU") AND
    (${CMAKE_CXX_COMPILER_VERSION} VERSION_LESS "9.0.0"))
        set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -no-integrated-cpp")
endif()

#### Enable C++11 (for std::shared_ptr support)
set(CMAKE_CXX_STANDARD 11)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

IF (WIN32)
	SET_PROPERTY(GLOBAL PROPERTY WIN32 "WIN32")
ENDIF(WIN32)

############## Code Coverage #########################
if (ENABLE_COVERAGE AND NOT ENABLE_TESTS)
  message(WARNING "ENABLE_COVERAGE requires unit tests, forcing ENABLE_TESTS")
  set(ENABLE_TESTS ON CACHE BOOL "Build unit tests (requires Catch2 or UnitTest++)" FORCE)
endif()

if (ENABLE_COVERAGE)
  if (NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE "Debug")
    message(STATUS "Coverage enabled, setting build type to Debug")
  endif()
  include(CodeCoverage)
  append_coverage_compiler_flags()
endif()
add_feature_info("Coverage" ENABLE_COVERAGE "analyze test coverage and generate report")

# Make sure we've picked some build type, default to release
if(NOT DEFINED CMAKE_BUILD_TYPE OR CMAKE_BUILD_TYPE STREQUAL "")
  set(CMAKE_BUILD_TYPE "Release")
endif()

###
### Process subdirectories
###
add_subdirectory(src)
add_subdirectory(examples)
add_subdirectory(bindings)

###
### Configure Version.h header
###
# (Note: This comes after the subdirectories, because it needs variables
#        set during the dependency discovery in src/CMakeLists.txt)
configure_file(src/OpenShotVersion.h.in src/OpenShotVersion.h @ONLY)
# We'll want that installed later
install(FILES ${CMAKE_CURRENT_BINARY_DIR}/src/OpenShotVersion.h
        DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}/libopenshot)


################### DOCUMENTATION ###################
# Find Doxygen (used for documentation)
set(DOCS_ENABLED FALSE) # Only set true if Doxygen is found and configured
if (ENABLE_DOCS)
  include(cmake/Modules/UseDoxygen.cmake)

  # Doxygen was found
  if (TARGET doc)
  	message(STATUS "Doxygen found, documentation target enabled")
  	set(DOCS_ENABLED TRUE)

    # Install docs, if the user builds them with `make doc`
    install(CODE "MESSAGE(\"Checking for documentation files to install...\")")
    install(CODE "MESSAGE(\"(Compile with 'make doc' command, requires Doxygen)\")")

    install(DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}/doc/html/
            DESTINATION ${CMAKE_INSTALL_DOCDIR}/API
            MESSAGE_NEVER # Don't spew about file copies
            OPTIONAL )    # No error if the docs aren't found
  endif()
endif()

############# PROCESS tests/ DIRECTORY ##############
find_package(Catch2 QUIET)
if(NOT Catch2_FOUND)
  set(BUILD_TESTING FALSE)
endif()
if(BUILD_TESTING)
  include(Catch)
  # Parallel tests seem to hang randomly on Windows
  if(ENABLE_PARALLEL_CTEST)
    if(WIN32)
      message(WARNING "Win32 detected, disabling parallel unit tests")
    else()
      # Figure out the amount of parallelism for CTest
      include(ProcessorCount)
      ProcessorCount(CPU_COUNT)
      if(CPU_COUNT GREATER 1)
        add_feature_info("Parallel tests" TRUE "Unit tests can use ${CPU_COUNT} processors")
        list(APPEND CTEST_OPTIONS "-j${CPU_COUNT}")
      endif()
    endif()
  endif()
  if(VERBOSE_TESTS)
    list(APPEND CTEST_OPTIONS "-VV")
  endif()
  list(APPEND CTEST_OPTIONS "--output-on-failure")
  add_subdirectory(tests)
endif()
add_feature_info("Unit tests" ${BUILD_TESTING} "Compile unit tests for library functions")

############## COVERAGE REPORTING #################
if (ENABLE_COVERAGE AND DEFINED UNIT_TEST_TARGETS)
  set(COVERAGE_EXCLUDES
    "bindings/*"
    "examples/*"
    "${CMAKE_CURRENT_BINARY_DIR}/bindings/*"
    "${CMAKE_CURRENT_BINARY_DIR}/src/*_autogen/*"
    "audio/*"
  )
  setup_target_for_coverage_lcov(
    NAME coverage
    LCOV_ARGS "--no-external"
    EXECUTABLE ctest
    EXECUTABLE_ARGS ${CTEST_OPTIONS}
    DEPENDENCIES openshot ${UNIT_TEST_TARGETS}
    EXCLUDE ${COVERAGE_EXCLUDES}
  )
  foreach(_t IN LISTS UNIT_TEST_NAMES)
    setup_target_for_coverage_lcov(
      NAME "${_t}_coverage"
      LCOV_ARGS "--no-external"
      EXECUTABLE ctest
      EXECUTABLE_ARGS ${CTEST_OPTIONS} -L "^${_t}$"
      DEPENDENCIES openshot openshot-${_t}-test
      EXCLUDE ${COVERAGE_EXCLUDES}
    )
  endforeach()
endif()

if(DEFINED UNIT_TEST_TARGETS AND NOT TARGET coverage)
  add_custom_target(coverage
    COMMAND ctest ${CTEST_OPTIONS}
    DEPENDS openshot ${UNIT_TEST_TARGETS}
    WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}
    COMMENT "Running unit tests (coverage disabled)"
  )
  foreach(_t IN LISTS UNIT_TEST_NAMES)
    add_custom_target("${_t}_coverage"
      COMMAND ctest ${CTEST_OPTIONS} -L "^${_t}$"
      DEPENDS openshot openshot-${_t}-test
      WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}
      COMMENT "Running unit tests for ${_t} class (coverage disabled)"
    )
  endforeach()
endif()

if(TARGET test AND NOT TARGET os_test)
  add_custom_target(os_test)
  add_dependencies(os_test coverage)
endif()

if(TARGET os_test AND NOT TARGET test AND CMAKE_VERSION VERSION_GREATER 3.11)
  # Also hook up 'test' as an alias for the 'os_test' target, if possible
  # This requires CMake 3.11+, where the CMP0037 policy
  # configured to 'NEW' mode will not reserve target names
  # unless the corresponding feature is actually used
  if (POLICY CMP0037)
  	cmake_policy(SET CMP0037 NEW)
  endif()
  message(STATUS "Cmake 3.11+ detected, enabling 'test' target")
  add_custom_target(test)
  add_dependencies(test os_test)
endif()

###
### Add feature-summary details on non-default built targets
###
set(optional_targets test os_test coverage doc)
set(target_test_description "Build and execute unit tests")
set(target_os_test_description "Build and execute unit tests (legacy target)")
set(target_coverage_description "Run unit tests and (if enabled) collect coverage data")
set(target_doc_description "Build formatted API documentation (HTML+SVG)")
foreach(_tname IN LISTS optional_targets)
  if(TARGET ${_tname})
    add_feature_info("Non-default target '${_tname}'" TRUE ${target_${_tname}_description})
  else()
    message(DEBUG "No target ${_tname}")
  endif()
endforeach()

########### PRINT FEATURE SUMMARY ##############
feature_summary(WHAT ALL
  INCLUDE_QUIET_PACKAGES
  FATAL_ON_MISSING_REQUIRED_PACKAGES
  DESCRIPTION "Build configuration:")
