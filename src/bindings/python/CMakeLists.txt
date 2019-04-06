####################### CMakeLists.txt (libopenshot) #########################
# @brief CMake build file for libopenshot (used to generate Python SWIG bindings)
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


############### SWIG PYTHON BINDINGS ################
FIND_PACKAGE(SWIG 2.0 REQUIRED)
INCLUDE(${SWIG_USE_FILE})

### Enable some legacy SWIG behaviors, in newer CMAKEs
if (CMAKE_VERSION VERSION_GREATER 3.13)
	cmake_policy(SET CMP0078 OLD)
endif()
if (CMAKE_VERSION VERSION_GREATER 3.14)
	cmake_policy(SET CMP0086 OLD)
endif()

FIND_PACKAGE(PythonLibs 3)
FIND_PACKAGE(PythonInterp 3)
if (PYTHONLIBS_FOUND AND PYTHONINTERP_FOUND)

	### Include Python header files
	INCLUDE_DIRECTORIES(${PYTHON_INCLUDE_PATH})
	INCLUDE_DIRECTORIES(${CMAKE_CURRENT_SOURCE_DIR})

	### Enable C++ support in SWIG
	set_property(SOURCE openshot.i PROPERTY CPLUSPLUS ON)
	set_property(SOURCE openshot.i PROPERTY SWIG_MODULE_NAME openshot)
	SET(CMAKE_SWIG_FLAGS "")

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
	target_link_libraries(${SWIG_MODULE_pyopenshot_REAL_NAME}
	                      ${PYTHON_LIBRARIES} openshot)

	### FIND THE PYTHON INTERPRETER (AND THE SITE PACKAGES FOLDER)
	execute_process ( COMMAND ${PYTHON_EXECUTABLE} -c "\
from distutils.sysconfig import get_python_lib; \
print( get_python_lib( plat_specific=True, prefix='${CMAKE_INSTALL_PREFIX}' ) )"
	                  OUTPUT_VARIABLE _ABS_PYTHON_MODULE_PATH
	                  OUTPUT_STRIP_TRAILING_WHITESPACE )

	GET_FILENAME_COMPONENT(_ABS_PYTHON_MODULE_PATH
	                       "${_ABS_PYTHON_MODULE_PATH}" ABSOLUTE)
	FILE(RELATIVE_PATH _REL_PYTHON_MODULE_PATH
	     ${CMAKE_INSTALL_PREFIX} ${_ABS_PYTHON_MODULE_PATH})
	SET(PYTHON_MODULE_PATH ${_REL_PYTHON_MODULE_PATH})

	############### INSTALL HEADERS & LIBRARY ################
	### Install Python bindings
	INSTALL(TARGETS ${SWIG_MODULE_pyopenshot_REAL_NAME}
	        LIBRARY DESTINATION ${PYTHON_MODULE_PATH} )
	INSTALL(FILES ${CMAKE_CURRENT_BINARY_DIR}/openshot.py
	        DESTINATION ${PYTHON_MODULE_PATH} )

endif ()
