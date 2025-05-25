####################### CMakeLists.txt (libopenshot) #########################
# @brief CMake build file for libopenshot (used to generate Python SWIG bindings)
# @author Jonathan Thomas <jonathan@openshot.org>
#
# @section LICENSE
#
# Copyright (c) 2008-2019 OpenShot Studios, LLC
#
# SPDX-License-Identifier: LGPL-3.0-or-later

############### SWIG PYTHON BINDINGS ################
find_package(SWIG 3.0 REQUIRED)
include(${SWIG_USE_FILE})

### Enable some legacy SWIG behaviors, in newer CMAKEs
if (POLICY CMP0078)
  cmake_policy(SET CMP0078 OLD)
endif()
if (POLICY CMP0086)
  cmake_policy(SET CMP0086 OLD)
endif()

find_package(PythonInterp 3)
find_package(PythonLibs 3)

if (NOT PYTHONLIBS_FOUND OR NOT PYTHONINTERP_FOUND)
  return()
endif()

### Include Python header files
include_directories(${PYTHON_INCLUDE_PATH})

if (CMAKE_VERSION VERSION_LESS 3.12)
  ### Include project headers
  include_directories(
    "${PROJECT_SOURCE_DIR}/src"
    "${PROJECT_BINARY_DIR}/src")
endif()

### Enable C++ in SWIG
set_property(SOURCE openshot.i PROPERTY CPLUSPLUS ON)
set_property(SOURCE openshot.i PROPERTY SWIG_MODULE_NAME openshot)

# Set the SWIG_FLAGS from the library target, IFF its
# COMPILE_DEFINITIONS property is set (in practice, always true)
if(CMAKE_VERSION VERSION_GREATER 3.15)
  set(_defs
    $<REMOVE_DUPLICATES:$<TARGET_PROPERTY:openshot,COMPILE_DEFINITIONS>>)
elseif(CMAKE_VERSION VERSION_GREATER 3.12)
  set(_defs $<TARGET_PROPERTY:openshot,COMPILE_DEFINITIONS>)
endif()
if(DEFINED _defs)
  set_property(SOURCE openshot.i PROPERTY
    COMPILE_DEFINITIONS ${_defs})
else()
  get_property(_defs TARGET openshot PROPERTY COMPILE_DEFINITIONS)
  foreach(_d ${_defs})
    list(APPEND _flags -D${_d})
  endforeach()
  set_property(SOURCE openshot.i PROPERTY
    SWIG_FLAGS ${_flags})
endif()

### Suppress a ton of warnings in the generated SWIG C++ code
set(SWIG_CXX_FLAGS "-Wno-unused-variable -Wno-unused-function \
  -Wno-deprecated-copy -Wno-class-memaccess -Wno-cast-function-type \
  -Wno-unused-parameter -Wno-catch-value -Wno-sign-compare -Wno-ignored-qualifiers")
separate_arguments(sw_flags UNIX_COMMAND ${SWIG_CXX_FLAGS})
set_property(SOURCE openshot.i PROPERTY GENERATED_COMPILE_OPTIONS ${sw_flags})

### Take include dirs from target
if(CMAKE_VERSION VERSION_GREATER 3.15)
  set(_inc $<REMOVE_DUPLICATES:$<TARGET_PROPERTY:openshot,INCLUDE_DIRECTORIES>>)
elseif(CMAKE_VERSION VERSION_GREATER 3.12)
  set(_inc $<TARGET_PROPERTY:openshot,INCLUDE_DIRECTORIES>)
endif()
if (DEFINED _inc)
  set_property(SOURCE openshot.i PROPERTY INCLUDE_DIRECTORIES ${_inc})
endif()

### (FINALLY!)
### Properly manage dependencies (regenerate bindings after changes)
if (CMAKE_VERSION VERSION_GREATER 3.20)
  set_property(SOURCE openshot.i PROPERTY USE_SWIG_DEPENDENCIES TRUE)
endif()

### Add the SWIG interface file (which defines all the SWIG methods)
if (CMAKE_VERSION VERSION_LESS 3.8.0)
  swig_add_module(pyopenshot python openshot.i)
else()
  swig_add_library(pyopenshot LANGUAGE python SOURCES openshot.i)
endif()

### Set output name of target
set_target_properties(${SWIG_MODULE_pyopenshot_REAL_NAME} PROPERTIES
  PREFIX "_" OUTPUT_NAME "openshot")

### Link the new python wrapper library with libopenshot
target_link_libraries(${SWIG_MODULE_pyopenshot_REAL_NAME} PUBLIC
  ${PYTHON_LIBRARIES} openshot)

######### INSTALL PATH ########
if (NOT DEFINED PYTHON_MODULE_PATH AND DEFINED $ENV{PYTHON_MODULE_PATH})
  set(PYTHON_MODULE_PATH $ENV{PYTHON_MODULE_PATH})
endif()

if (NOT DEFINED PYTHON_MODULE_PATH)
  if (WIN32 OR APPLE)
    set (PYTHON_MODULE_PATH "python")
  endif()

  if (UNIX AND NOT APPLE)
    ### Check if the following Debian-friendly python module path exists
    set(PYTHON_MODULE_PATH
      "lib/python${PYTHON_VERSION_MAJOR}.${PYTHON_VERSION_MINOR}/dist-packages")

    if (NOT EXISTS "${CMAKE_INSTALL_PREFIX}/${PYTHON_MODULE_PATH}")
      ### Calculate the python module path (prefer sysconfig, fallback to distutils for compatibility)
      execute_process(COMMAND ${PYTHON_EXECUTABLE} -c "\
try:
  import sysconfig; print(sysconfig.get_path('platlib'))
except ImportError:
  from distutils.sysconfig import get_python_lib; \
  print(get_python_lib(plat_specific=True, prefix=''))"
              OUTPUT_VARIABLE PYTHON_MODULE_PATH
              OUTPUT_STRIP_TRAILING_WHITESPACE)
    endif()
  endif()
endif()

message(STATUS "PYTHON_MODULE_PATH: ${CMAKE_INSTALL_PREFIX}/${PYTHON_MODULE_PATH}")

############### INSTALL HEADERS & LIBRARY ################
### Install Python bindings
install(TARGETS ${SWIG_MODULE_pyopenshot_REAL_NAME}
  DESTINATION ${PYTHON_MODULE_PATH} )
install(FILES ${CMAKE_CURRENT_BINARY_DIR}/openshot.py
  DESTINATION ${PYTHON_MODULE_PATH} )
