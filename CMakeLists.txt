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

cmake_minimum_required(VERSION 3.2...3.14 FATAL_ERROR)

message("\
-----------------------------------------------------------------
          Welcome to the OpenShot Build System!

CMake will now check libopenshot's build dependencies and inform
you of any missing files or other issues.

For more information, please visit <http://www.openshot.org/>.
-----------------------------------------------------------------")

################ LOCAL CMAKE MODULES ##################
set(CMAKE_MODULE_PATH "${CMAKE_SOURCE_DIR}/cmake/Modules")

################ PROJECT VERSION ####################
set(PROJECT_VERSION_FULL "0.2.3-dev1")
set(PROJECT_SO_VERSION 17)

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

# CTest testing framework
include(CTest)

# Collect and display summary of options/dependencies
include(FeatureSummary)

################ OPTIONS ##################
# Optional build settings for libopenshot
option(USE_SYSTEM_JSONCPP "Use system installed JsonCpp, if found" ON)
option(DISABLE_BUNDLED_JSONCPP "Don't fall back to bundled JsonCpp" OFF)
option(DISABLE_TESTS "Don't build unit tests" OFF)
option(ENABLE_IWYU "Enable 'Include What You Use' scanner (CMake 3.3+)" OFF)
option(ENABLE_COVERAGE "Enable coverage reporting" OFF)

########## Configure Version.h header ##############
configure_file(include/OpenShotVersion.h.in include/OpenShotVersion.h @ONLY)
# We'll want that installed later
install(FILES ${CMAKE_CURRENT_BINARY_DIR}/include/OpenShotVersion.h
        DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}/libopenshot)

#### Enable C++11 (for std::shared_ptr support)
set(CMAKE_CXX_STANDARD 11)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

include_directories(
  ${CMAKE_CURRENT_SOURCE_DIR}/include
  ${CMAKE_CURRENT_BINARY_DIR}/include)

############## Code Coverage #########################
if (DISABLE_TESTS AND ENABLE_COVERAGE)
  message(WARNING "ENABLE_COVERAGE requires tests, overriding DISABLE_TESTS")
  set(DISABLE_TESTS OFF CACHE BOOL "Don't build unit tests" FORCE)
endif()

if (ENABLE_COVERAGE)
  if (NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE "Debug")
    message(STATUS "Coverage enabled, set missing CMAKE_BUILD_TYPE to Debug")
  endif()
  include(CodeCoverage)
  append_coverage_compiler_flags()
endif()
add_feature_info("Coverage" ENABLE_COVERAGE "analyze test coverage and generate report")

############## PROCESS src/ DIRECTORIES ##############
add_subdirectory(src)

################### DOCUMENTATION ###################
# Find Doxygen (used for documentation)
include(cmake/Modules/UseDoxygen.cmake)

# Doxygen was found
if (TARGET doc)
	message(STATUS "Doxygen found, documentation target enabled")
	message("\nTo compile documentation in doc/html, run: 'make doc'")

  # Install docs, if the user builds them with `make doc`
  install(CODE "MESSAGE(\"Checking for documentation files to install...\")")
  install(CODE "MESSAGE(\"(Compile with 'make doc' command, requires Doxygen)\")")

  install(DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}/doc/html/
          DESTINATION ${CMAKE_INSTALL_DOCDIR}/API
          MESSAGE_NEVER # Don't spew about file copies
          OPTIONAL )    # No error if the docs aren't found
endif()

############# PROCESS tests/ DIRECTORY ##############
if(NOT DISABLE_TESTS OR ENABLE_COVERAGE)
  enable_testing()
  add_subdirectory(tests)
  set(TESTS_ENABLED TRUE)
endif()
add_feature_info("Unit tests" TESTS_ENABLED "run a series of unit tests on library functions")

############## COVERAGE REPORTING #################
if (ENABLE_COVERAGE AND COVERAGE_TARGETS)
  add_custom_target(coverage
    COMMAND ${CMAKE_MAKE_PROGRAM} os_test
    COMMENT "Generate coverage reports with 'make coverage'")
  add_dependencies(coverage ${COVERAGE_TARGETS})
endif()

########### PRINT FEATURE SUMMARY ##############
feature_summary(WHAT ALL
    INCLUDE_QUIET_PACKAGES
    FATAL_ON_MISSING_REQUIRED_PACKAGES
    DESCRIPTION "Displaying feature summary\n\nBuild configuration:")
