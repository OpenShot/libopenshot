####################### CMakeLists.txt (libopenshot) #########################
# @brief CMake build file for libopenshot (used to generate makefiles)
# @author Jonathan Thomas <jonathan@openshot.org>
#
# @section LICENSE
#
# Copyright (c) 2008-2014 OpenShot Studios, LLC
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

cmake_minimum_required(VERSION 2.8.11)

MESSAGE("--------------------------------------------------------------")
MESSAGE("Welcome to the OpenShot Build System! CMake will now check for all required build")
MESSAGE("dependencies and notify you of any missing files or other issues. If you have any")
MESSAGE("questions or issues, please visit <http://www.openshot.org/>.")

################ ADD CMAKE MODULES ##################
set(CMAKE_MODULE_PATH "${CMAKE_SOURCE_DIR}/cmake/Modules")

################ GET VERSION INFORMATION FROM VERSION.H ##################
MESSAGE("--------------------------------------------------------------")
MESSAGE("Determining Version Number (from Version.h file)")

#### Get the lines related to libopenshot version from the Version.h header
file(STRINGS ${CMAKE_CURRENT_SOURCE_DIR}/include/Version.h OPENSHOT_VERSION_LINES
  REGEX "#define[ ]+OPENSHOT_VERSION_.*[0-9]+;.*")
  
#### Set each line into it's own variable
list (GET OPENSHOT_VERSION_LINES 0 LINE_MAJOR)
list (GET OPENSHOT_VERSION_LINES 1 LINE_MINOR)
list (GET OPENSHOT_VERSION_LINES 2 LINE_BUILD)
list (GET OPENSHOT_VERSION_LINES 3 LINE_SO)

#### Get the version number out of each line
STRING(REGEX REPLACE "#define[ ]+OPENSHOT_VERSION_MAJOR.*([0-9])+;(.*)" "\\1" MAJOR_VERSION "${LINE_MAJOR}")
STRING(REGEX REPLACE "#define[ ]+OPENSHOT_VERSION_MINOR.*([0-9])+;(.*)" "\\1" MINOR_VERSION "${LINE_MINOR}")
STRING(REGEX REPLACE "#define[ ]+OPENSHOT_VERSION_BUILD.*([0-9])+;(.*)" "\\1" BUILD_VERSION "${LINE_BUILD}")
STRING(REGEX REPLACE "#define[ ]+OPENSHOT_VERSION_SO.*([0-9])+;(.*)" "\\1" SO_VERSION "${LINE_SO}")
set(PROJECT_VERSION "${MAJOR_VERSION}.${MINOR_VERSION}.${BUILD_VERSION}")

MESSAGE("--> MAJOR Version: ${MAJOR_VERSION}")
MESSAGE("--> MINOR Version: ${MINOR_VERSION}")
MESSAGE("--> BUILD Version: ${BUILD_VERSION}")
MESSAGE("--> SO/API/ABI Version: ${SO_VERSION}")
MESSAGE("--> VERSION: ${PROJECT_VERSION}")
MESSAGE("")

################### SETUP PROJECT ###################
PROJECT(openshot)
MESSAGE("--------------------------------------------------------------")
MESSAGE("Generating build files for ${PROJECT_NAME} (${PROJECT_VERSION})")

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
INSTALL(FILES ${doc_files} DESTINATION share/doc/libopenshot)