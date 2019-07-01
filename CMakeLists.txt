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

cmake_minimum_required(VERSION 3.1...3.14 FATAL_ERROR)

message("\
-----------------------------------------------------------------
          Welcome to the OpenShot Build System!

CMake will now check libopenshot's build dependencies and inform
you of any missing files or other issues.

For more information, please visit <http://www.openshot.org/>.
-----------------------------------------------------------------")

################ ADD CMAKE MODULES ##################
set(CMAKE_MODULE_PATH "${CMAKE_SOURCE_DIR}/cmake/Modules")
# Define install paths according to system conventions
include(GNUInstallDirs)

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
Generating build files for OpenShot
  Building ${PROJECT_NAME} (version ${PROJECT_VERSION})
  SO/API/ABI Version: ${PROJECT_SO_VERSION}
")

#### Work around a GCC < 9 bug with handling of _Pragma() in macros
#### See https://gcc.gnu.org/bugzilla/show_bug.cgi?id=55578
if ((${CMAKE_CXX_COMPILER_ID} STREQUAL "GNU") AND
    (${CMAKE_CXX_COMPILER_VERSION} VERSION_LESS "9.0.0"))
        set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -no-integrated-cpp")
endif()

########## Configure Version.h header ##############
configure_file(include/OpenShotVersion.h.in include/OpenShotVersion.h @ONLY)
# We'll want that installed later
install(FILES ${CMAKE_CURRENT_BINARY_DIR}/include/OpenShotVersion.h
        DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}/libopenshot)


############### Set up include paths #################
list(APPEND OPENSHOT_INCLUDE_DIRS
    include
    include/effects
    include/Qt
    ${CMAKE_CURRENT_BINARY_DIR}/include )

########## Configure Version.h header ##############
configure_file(include/OpenShotVersion.h.in include/OpenShotVersion.h @ONLY)
# We'll want that installed later
install(FILES ${CMAKE_CURRENT_BINARY_DIR}/include/OpenShotVersion.h
        DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}/libopenshot)


############### Set up include paths #################
list(APPEND OPENSHOT_INCLUDE_DIRS
    include
    include/effects
    include/Qt
    ${CMAKE_CURRENT_BINARY_DIR}/include )

#### Enable C++11 (for std::shared_ptr support)
set(CMAKE_CXX_STANDARD 11)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

IF (WIN32)
	SET_PROPERTY(GLOBAL PROPERTY WIN32 "WIN32")
ENDIF(WIN32)

############## FIND ALL QT RELATED HEADERS ##############
set(QT_HEADER_DIR ${CMAKE_CURRENT_SOURCE_DIR}/include/Qt)
FILE(GLOB QT_HEADER_FILES "${QT_HEADER_DIR}/*.h")

############## PROCESS SUB-DIRECTORIES ##############
add_subdirectory(src)
add_subdirectory(tests)

################### DOCUMENTATION ###################
# Find Doxygen (used for documentation)
include(cmake/Modules/UseDoxygen.cmake)

file(GLOB_RECURSE doc_files ${CMAKE_CURRENT_BINARY_DIR}/doc/html/*.*)
INSTALL(FILES ${doc_files} DESTINATION ${CMAKE_INSTALL_DOCDIR})
