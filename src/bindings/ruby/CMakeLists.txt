####################### CMakeLists.txt (libopenshot) #########################
# @brief CMake build file for libopenshot (used to generate Ruby SWIG bindings)
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


############### RUBY BINDINGS ################
FIND_PACKAGE(SWIG 2.0 REQUIRED)
INCLUDE(${SWIG_USE_FILE})

FIND_PACKAGE(Ruby)
IF (RUBY_FOUND)

	### Include the Ruby header files
	INCLUDE_DIRECTORIES(${RUBY_INCLUDE_DIRS})
	INCLUDE_DIRECTORIES(${CMAKE_CURRENT_SOURCE_DIR})
	
	### Enable C++ in SWIG
	SET_SOURCE_FILES_PROPERTIES(openshot.i PROPERTIES CPLUSPLUS ON)
	SET(CMAKE_SWIG_FLAGS "")

	### Add the SWIG interface file (which defines all the SWIG methods)
	SWIG_ADD_MODULE(rbopenshot ruby openshot.i)
	
	### Set name of target (with no prefix, since Ruby does not like that)
	SET_TARGET_PROPERTIES(rbopenshot PROPERTIES PREFIX "" OUTPUT_NAME "openshot")

	### Link the new Ruby wrapper library with libopenshot
	SWIG_LINK_LIBRARIES(rbopenshot ${RUBY_LIBRARY} openshot)
	
	### FIND THE RUBY INTERPRETER (AND THE LOAD_PATH FOLDER)
	EXECUTE_PROCESS(COMMAND ${RUBY_EXECUTABLE} -r rbconfig -e "print RbConfig::CONFIG['vendorarchdir']" OUTPUT_VARIABLE RUBY_VENDOR_ARCH_DIR)
	MESSAGE(STATUS "Ruby executable: ${RUBY_EXECUTABLE}")
	MESSAGE(STATUS "Ruby vendor arch dir: ${RUBY_VENDOR_ARCH_DIR}")
	MESSAGE(STATUS "Ruby include path: ${RUBY_INCLUDE_PATH}")
	

	############### INSTALL HEADERS & LIBRARY ################
	# Install Ruby bindings
	INSTALL(TARGETS rbopenshot LIBRARY DESTINATION ${RUBY_VENDOR_ARCH_DIR})
	
ENDIF (RUBY_FOUND)
