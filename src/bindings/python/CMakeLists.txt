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

FIND_PACKAGE(PythonLibs 3)
FIND_PACKAGE(PythonInterp 3)
IF (PYTHONLIBS_FOUND)
	IF (PYTHONINTERP_FOUND)

		### Include Python header files
		INCLUDE_DIRECTORIES(${PYTHON_INCLUDE_PATH})
		INCLUDE_DIRECTORIES(${CMAKE_CURRENT_SOURCE_DIR})
		
		### Enable C++ support in SWIG
		SET_SOURCE_FILES_PROPERTIES(openshot.i PROPERTIES CPLUSPLUS ON)
		SET(CMAKE_SWIG_FLAGS "")
		
		### Add the SWIG interface file (which defines all the SWIG methods)
		SWIG_ADD_MODULE(openshot python openshot.i)

		### Link the new python wrapper library with libopenshot
		SWIG_LINK_LIBRARIES(openshot ${PYTHON_LIBRARIES} openshot)

		### FIND THE PYTHON INTERPRETER (AND THE SITE PACKAGES FOLDER)
		EXECUTE_PROCESS ( COMMAND ${PYTHON_EXECUTABLE} -c "import site; print(site.getsitepackages()[0])"
		                     OUTPUT_VARIABLE _ABS_PYTHON_MODULE_PATH
		                     OUTPUT_STRIP_TRAILING_WHITESPACE )
		GET_FILENAME_COMPONENT(_ABS_PYTHON_MODULE_PATH "${_ABS_PYTHON_MODULE_PATH}" ABSOLUTE)
		FILE(RELATIVE_PATH _REL_PYTHON_MODULE_PATH ${CMAKE_INSTALL_PREFIX} ${_ABS_PYTHON_MODULE_PATH})
		SET(PYTHON_MODULE_PATH ${_REL_PYTHON_MODULE_PATH})
		
		
		############### INSTALL HEADERS & LIBRARY ################
		### Install Python bindings
		INSTALL(TARGETS _openshot DESTINATION ${PYTHON_MODULE_PATH} )
		INSTALL(FILES ${CMAKE_CURRENT_BINARY_DIR}/openshot.py DESTINATION ${PYTHON_MODULE_PATH} )

	ENDIF(PYTHONINTERP_FOUND)
ENDIF (PYTHONLIBS_FOUND)