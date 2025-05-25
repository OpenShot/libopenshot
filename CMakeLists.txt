####################### CMakeLists.txt (libopenshot) #########################
# @brief CMake build file for libopenshot (used to generate makefiles)
# @author Jonathan Thomas <jonathan@openshot.org>
#
# @section LICENSE
#
# Copyright (c) 2008-2019 OpenShot Studios, LLC
#
# SPDX-License-Identifier: LGPL-3.0-or-later

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
set(PROJECT_VERSION_FULL "0.4.0")
set(PROJECT_SO_VERSION 27)

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

option(ENABLE_LIB_DOCS "Build API documentation (requires Doxygen)" ON)

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

### JUCE requires one of -DDEBUG or -DNDEBUG set on the
### compile command line. CMake automatically sets -DNDEBUG
### on all non-debug configs, so we'll just add -DDEBUG to
### the debug build flags
set(CMAKE_CXX_FLAGS_DEBUG "${CMAKE_CXX_FLAGS_DEBUG} -DDEBUG")

#### Work around a GCC < 9 bug with handling of _Pragma() in macros
#### See https://gcc.gnu.org/bugzilla/show_bug.cgi?id=55578
if ((${CMAKE_CXX_COMPILER_ID} STREQUAL "GNU") AND
    (${CMAKE_CXX_COMPILER_VERSION} VERSION_LESS "9.0.0"))
        set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -no-integrated-cpp")
endif()

#### Set C++ standard level
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

IF (WIN32)
	SET_PROPERTY(GLOBAL PROPERTY WIN32 "WIN32")
ENDIF(WIN32)

############## Code Coverage #########################
if (ENABLE_COVERAGE AND NOT BUILD_TESTING)
  message(WARNING "ENABLE_COVERAGE requires unit tests, forcing BUILD_TESTING")
  set(BUILD_TESTING ON CACHE BOOL "Build unit tests (requires Catch2 or UnitTest++)" FORCE)
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
# Legacy option support
if(DEFINED CACHE{ENABLE_DOCS})
  set(ENABLE_LIB_DOCS ${ENABLE_DOCS} CACHE BOOL "Build API documentation (requires Doxygen)")
  unset(ENABLE_DOCS CACHE)
  message(WARNING "The ENABLE_DOCS option is deprecated, please use ENABLE_LIB_DOCS")
endif()

# Find Doxygen (used for documentation)
set(CAN_BUILD_LIB_DOCS FALSE) # Only set true if Doxygen is found and configured
if (ENABLE_LIB_DOCS AND CMAKE_VERSION VERSION_GREATER 3.11)
  find_package(Doxygen)
  if(Doxygen_FOUND)
  	message(STATUS "Doxygen found, enabling ${PROJECT_NAME} documentation target")
    set(DOXYGEN_QUIET YES)
    set(DOXYGEN_PROJECT_NAME "OpenShot Library | libopenshot")
    set(DOXYGEN_PROJECT_NUMBER ${PROJECT_VERSION})
    set(DOXYGEN_STRIP_FROM_PATH "${PROJECT_SOURCE_DIR}/src")
    set(DOXYGEN_STRIP_FROM_INC_PATH "${PROJECT_SOURCE_DIR}/src")
    set(DOXYGEN_INCLUDE_FILE_PATTERNS "*.h")
    set(DOXYGEN_PREDEFINED
      USE_BLACKMAGIC
      USE_IMAGEMAGICK
      USE_OPENCV
      USE_BABL
      USE_RESVG
      TMS_DEP_MSG:=0
    )
    set(DOXYGEN_IMAGE_PATH "${PROJECT_SOURCE_DIR}/doc/images")
    set(DOXYGEN_OUTPUT_DIRECTORY "doc")

    set(DOXYGEN_EXTRACT_ALL YES)
    set(DOXYGEN_SORT_BRIEF_DOCS YES)
    set(DOXYGEN_RECURSIVE YES)
    set(DOXYGEN_SOURCE_BROWSER YES)
    set(DOXYGEN_REFERENCED_BY_RELATION YES)

    set(DOXYGEN_HTML_OUTPUT "html")
    set(DOXYGEN_HTML_DYNAMIC_SECTIONS NO)

    set(DOXYGEN_DOT_IMAGE_FORMAT svg)
    set(DOXYGEN_INTERACTIVE_SVG YES)
    set(DOXYGEN_DOT_TRANSPARENT YES)

    doxygen_add_docs(${PROJECT_NAME}-doc
      ${PROJECT_SOURCE_DIR}/src
      ${PROJECT_SOURCE_DIR}/doc
      ${PROJECT_BINARY_DIR}/src/OpenShotVersion.h
      WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}
      COMMENT "Generate ${PROJECT_NAME} documentation"
    )
  endif()
endif()
if (TARGET ${PROJECT_NAME}-doc)
	set(CAN_BUILD_LIB_DOCS TRUE)

  # Install docs, if the user builds them
  install(CODE "MESSAGE(\"Checking for documentation files to install...\")")
  install(DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}/doc/html/
    DESTINATION ${CMAKE_INSTALL_DOCDIR}/API
    MESSAGE_NEVER # Don't spew about file copies
    OPTIONAL )    # No error if the docs aren't found

  if(NOT TARGET doc)
    add_custom_target(doc)
  endif()
  if(TARGET doc)
    add_dependencies(doc ${PROJECT_NAME}-doc)
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
    "**/*_wrap.cxx"
    "examples/*"
    "**/moc_*.cpp"
    "*.pb.cc"
    "*.pb.h"
    "src/*.pb.cc"
    "src/*.pb.h"
    "audio/*"
  )
  setup_target_for_coverage_lcov(
    NAME coverage
    LCOV_ARGS ""
    EXECUTABLE ctest
    EXECUTABLE_ARGS ${CTEST_OPTIONS}
    DEPENDENCIES openshot ${UNIT_TEST_TARGETS}
    EXCLUDE ${COVERAGE_EXCLUDES}
  )
  foreach(_t IN LISTS UNIT_TEST_NAMES)
    setup_target_for_coverage_lcov(
      NAME "${_t}_coverage"
      LCOV_ARGS ""
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
  DESCRIPTION "${PROJECT_NAME} Build configuration:")
