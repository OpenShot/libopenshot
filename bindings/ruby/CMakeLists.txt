####################### CMakeLists.txt (libopenshot) #########################
# @brief CMake build file for libopenshot (used to generate Ruby SWIG bindings)
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

############### RUBY BINDINGS ################
find_package(SWIG 3.0 REQUIRED)
include(${SWIG_USE_FILE})

### Enable some legacy SWIG behaviors, in newer CMAKEs
if (POLICY CMP0078)
	cmake_policy(SET CMP0078 OLD)
endif()
if (POLICY CMP0086)
	cmake_policy(SET CMP0086 OLD)
endif()

find_package(Ruby)
if (NOT RUBY_FOUND)
  return()
endif()

### Ruby 2.7.0 made API changes that are incompatible with versions of
### SWIG prior to 4.0.3
option(SILENCE_RUBY_VERSION_WARNING
  "Don't warn about possible SWIG incompatibilities with Ruby 2.7.0+" OFF)

if (${RUBY_VERSION} VERSION_GREATER 2.6.9 AND ${SWIG_VERSION} VERSION_LESS 4.0.2)
  if (NOT ${SILENCE_RUBY_VERSION_WARNING})
    message(WARNING "\
Ruby 2.7.0+ detected, building the libopenshot Ruby API bindings \
requires either SWIG 4.0.2+ or an older version patched with this commit: \
https://github.com/swig/swig/commit/5542cc228ad10bdc5c91107afb77c808c43bf2a4 \
(Recent Fedora and Ubuntu distro packages of SWIG 4.0.1 have already been \
patched.)")
    message(STATUS "To disable the previous warning, add \
-DSILENCE_RUBY_VERSION_WARNING:BOOL=1 to the cmake command line, \
or enable the option in the CMake GUI.")
  endif()
endif()

### Include the Ruby header files
include_directories(${RUBY_INCLUDE_DIRS})

if (CMAKE_VERSION VERSION_LESS 3.12)
  ### Include project headers
  include_directories(
    "${PROJECT_SOURCE_DIR}/src"
    "${PROJECT_BINARY_DIR}/src")
endif()

### Enable C++ in SWIG
set_property(SOURCE openshot.i PROPERTY CPLUSPLUS ON)
set_property(SOURCE openshot.i PROPERTY SWIG_MODULE_NAME openshot)

### Suppress a ton of warnings in the generated SWIG C++ code
set(SWIG_CXX_FLAGS "-Wno-unused-variable -Wno-unused-function \
  -Wno-deprecated-copy -Wno-class-memaccess -Wno-cast-function-type \
  -Wno-unused-parameter -Wno-catch-value -Wno-sign-compare -Wno-ignored-qualifiers")
separate_arguments(sw_flags UNIX_COMMAND ${SWIG_CXX_FLAGS})
set_property(SOURCE openshot.i PROPERTY GENERATED_COMPILE_OPTIONS ${sw_flags})

### Take include dirs from target, automatically if possible
if (CMAKE_VERSION VERSION_GREATER 3.13)
	set_property(SOURCE openshot.i PROPERTY USE_TARGET_INCLUDE_DIRECTORIES True)
else ()
	set_property(SOURCE openshot.i PROPERTY
    INCLUDE_DIRECTORIES $<TARGET_PROPERTY:openshot,INCLUDE_DIRECTORIES>)
endif ()

### Add the SWIG interface file (which defines all the SWIG methods)
if (CMAKE_VERSION VERSION_LESS 3.8.0)
	swig_add_module(rbopenshot ruby openshot.i)
else()
	swig_add_library(rbopenshot LANGUAGE ruby SOURCES openshot.i)
endif()

### Set name of target (with no prefix, since Ruby does not like that)
# XXX: If this is not done exactly this way, the module builds as
#      e.g. rbopenshot.so, but its initializer method will be named
#      'Init_openshot()' (via the module name set in the SWIG .i file).
#      Which leads to Ruby barfing when it attempts to load the module.
set_target_properties(${SWIG_MODULE_rbopenshot_REAL_NAME} PROPERTIES
  PREFIX "" OUTPUT_NAME "openshot")

### Link the new Ruby wrapper library with libopenshot
target_link_libraries(${SWIG_MODULE_rbopenshot_REAL_NAME} PUBLIC
    ${RUBY_LIBRARY} openshot)

######### INSTALL PATH ########
if (NOT DEFINED RUBY_MODULE_PATH AND DEFINED $ENV{RUBY_MODULE_PATH})
  set(RUBY_MODULE_PATH $ENV{RUBY_MODULE_PATH})
endif()

if (NOT DEFINED RUBY_MODULE_PATH)
  if (WIN32 OR APPLE)
    set (RUBY_MODULE_PATH "ruby")
  endif()

  if (UNIX AND NOT APPLE)
    ### FIND THE RUBY INTERPRETER (AND THE LOAD_PATH FOLDER)
    execute_process(COMMAND ${RUBY_EXECUTABLE} -r rbconfig
      -e "dir = RbConfig::CONFIG['vendorarchdir']"
      -e "dir.start_with?(RbConfig::CONFIG['prefix']) && dir.sub!(RbConfig::CONFIG['prefix']+'/', '')"
      -e "p dir"
      OUTPUT_VARIABLE RUBY_MODULE_PATH
      OUTPUT_STRIP_TRAILING_WHITESPACE )
    # Ruby quotes its output strings
    string(REPLACE "\"" "" RUBY_MODULE_PATH "${RUBY_MODULE_PATH}")
  endif()
endif()

message(STATUS "RUBY_MODULE_PATH: ${CMAKE_INSTALL_PREFIX}/${RUBY_MODULE_PATH}")

############### INSTALL HEADERS & LIBRARY ################
# Install Ruby bindings
install(TARGETS ${SWIG_MODULE_rbopenshot_REAL_NAME}
  DESTINATION ${RUBY_MODULE_PATH} )
