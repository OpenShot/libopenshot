##################### tests/CMakeLists.txt (libopenshot) ######################
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

# Pick up our include directories from the parent context
include_directories(${OPENSHOT_INCLUDE_DIRS})

SET(TEST_MEDIA_PATH "${PROJECT_SOURCE_DIR}/src/examples/")

################ WINDOWS ##################
# Set some compiler options for Windows
# required for libopenshot-audio headers
IF (WIN32)
	STRING(REPLACE "/" "\\\\" TEST_MEDIA_PATH TEST_MEDIA_PATH)
	add_definitions( -DIGNORE_JUCE_HYPOT=1 )
	SET(CMAKE_CXX_FLAGS " ${CMAKE_CXX_FLAGS} -include cmath")
ENDIF(WIN32)

add_definitions( -DTEST_MEDIA_PATH="${TEST_MEDIA_PATH}" )

################### UNITTEST++ #####################
# Find UnitTest++ libraries (used for unit testing)
FIND_PACKAGE(UnitTest++ REQUIRED)

# Include UnitTest++ headers (needed for compile)
include_directories(${UNITTEST++_INCLUDE_DIR})

################ IMAGE MAGICK ##################
# Set the Quantum Depth that ImageMagick was built with (default to 16 bits)
IF (MAGICKCORE_QUANTUM_DEPTH)
	add_definitions( -DMAGICKCORE_QUANTUM_DEPTH=${MAGICKCORE_QUANTUM_DEPTH} )
ELSE (MAGICKCORE_QUANTUM_DEPTH)
	add_definitions( -DMAGICKCORE_QUANTUM_DEPTH=16 )
ENDIF (MAGICKCORE_QUANTUM_DEPTH)
IF (MAGICKCORE_HDRI_ENABLE)
	add_definitions( -DMAGICKCORE_HDRI_ENABLE=${MAGICKCORE_HDRI_ENABLE} )
ELSE (MAGICKCORE_HDRI_ENABLE)
	add_definitions( -DMAGICKCORE_HDRI_ENABLE=0 )
ENDIF (MAGICKCORE_HDRI_ENABLE)
IF (OPENSHOT_IMAGEMAGICK_COMPATIBILITY)
	add_definitions( -DOPENSHOT_IMAGEMAGICK_COMPATIBILITY=${OPENSHOT_IMAGEMAGICK_COMPATIBILITY} )
ELSE (OPENSHOT_IMAGEMAGICK_COMPATIBILITY)
	add_definitions( -DOPENSHOT_IMAGEMAGICK_COMPATIBILITY=0 )
ENDIF (OPENSHOT_IMAGEMAGICK_COMPATIBILITY)

# Find the ImageMagick++ library
FIND_PACKAGE(ImageMagick COMPONENTS Magick++ MagickWand MagickCore)
IF (ImageMagick_FOUND)
	# Include ImageMagick++ headers (needed for compile)
	include_directories(${ImageMagick_INCLUDE_DIRS})

	# define a global var (used in the C++)
	add_definitions( -DUSE_IMAGEMAGICK=1 )
	SET(CMAKE_SWIG_FLAGS "-DUSE_IMAGEMAGICK=1")

ENDIF (ImageMagick_FOUND)

################# LIBOPENSHOT-AUDIO ###################
# Find JUCE-based openshot Audio libraries
FIND_PACKAGE(OpenShotAudio 0.1.8 REQUIRED)

# Include Juce headers (needed for compile)
include_directories(${LIBOPENSHOT_AUDIO_INCLUDE_DIRS})

################# QT5 ###################
# Find QT5 libraries
foreach(qt_lib Qt5Widgets Qt5Core Qt5Gui Qt5Multimedia Qt5MultimediaWidgets)
    find_package(${qt_lib} REQUIRED)
    # Header files
    list(APPEND QT_INCLUDES ${${qt_lib}_INCLUDE_DIRS})
    # Compiler definitions
    add_definitions(${${qt_lib}_DEFINITIONS})
    # Other CFLAGS
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} ${${qt_lib}_EXECUTABLE_COMPILE_FLAGS}")
endforeach()
list(REMOVE_DUPLICATES QT_INCLUDES)
include_directories(${QT_INCLUDES})

# Manually moc Qt files
qt5_wrap_cpp(MOC_FILES ${QT_HEADER_FILES})


################# BLACKMAGIC DECKLINK ###################
IF (ENABLE_BLACKMAGIC)
	# Find BlackMagic DeckLinkAPI libraries
	FIND_PACKAGE(BlackMagic)

	IF (BLACKMAGIC_FOUND)
		# Include Blackmagic headers (needed for compile)
		include_directories(${BLACKMAGIC_INCLUDE_DIR})
	ENDIF (BLACKMAGIC_FOUND)
ENDIF (ENABLE_BLACKMAGIC)


################### ZEROMQ #####################
# Find ZeroMQ library (used for socket communication & logging)

# Some platforms package the header-only cppzmq C++ bindings separately,
# others (Ubuntu) bundle them in with libzmq itself
find_package(cppzmq QUIET)
FIND_PACKAGE(ZMQ REQUIRED)

# Include ZeroMQ headers (needed for compile)
include_directories(${ZMQ_INCLUDE_DIRS})
if (cppzmq_FOUND)
    include_directories(${cppzmq_INCLUDE_DIRS})
endif()

################### RESVG #####################
# Find resvg library (used for rendering svg files)
FIND_PACKAGE(RESVG)

# Include resvg headers (optional SVG library)
if (RESVG_FOUND)
	include_directories(${RESVG_INCLUDE_DIRS})
endif(RESVG_FOUND)

################### JSONCPP #####################
# Include jsoncpp headers (needed for JSON parsing)
if (USE_SYSTEM_JSONCPP)
	find_package(JsonCpp REQUIRED)
	include_directories(${JSONCPP_INCLUDE_DIRS})
else()
	include_directories("../thirdparty/jsoncpp")
endif(USE_SYSTEM_JSONCPP)

IF (NOT DISABLE_TESTS)
	###############  SET TEST SOURCE FILES  #################
	SET ( OPENSHOT_TEST_FILES
		   Cache_Tests.cpp
		   Clip_Tests.cpp
		   Color_Tests.cpp
		   Coordinate_Tests.cpp
		   ReaderBase_Tests.cpp
		   ImageWriter_Tests.cpp
		   FFmpegReader_Tests.cpp
		   FFmpegWriter_Tests.cpp
		   Fraction_Tests.cpp
		   FrameMapper_Tests.cpp
		   KeyFrame_Tests.cpp
		   Point_Tests.cpp
		   Settings_Tests.cpp
		   Timeline_Tests.cpp )

	################ TESTER EXECUTABLE #################
	# Create unit test executable (openshot-test)
	message (STATUS "Tests enabled, test executable will be built as tests/openshot-test")
	add_executable(openshot-test
				   tests.cpp
				   ${OPENSHOT_TEST_FILES} )

	# Link libraries to the new executable
	target_link_libraries(openshot-test openshot ${UNITTEST++_LIBRARY})

	##### RUNNING TESTS (make os_test / make test) #####
	# Hook up the 'make os_test' target to the 'openshot-test' executable
	ADD_CUSTOM_TARGET(os_test COMMAND openshot-test)
	list(APPEND OS_TEST_CMDS "'make os_test'")

	# Also hook up 'make test', if possible
	# This requires CMake 3.11+, where the CMP0037 policy
	# configured to 'NEW' mode will not reserve target names
	# unless the corresponding feature is actually used
	if (POLICY CMP0037)
		cmake_policy(SET CMP0037 NEW)
	endif()
	if (CMAKE_VERSION VERSION_GREATER 3.11)
		message(STATUS "Cmake 3.11+ detected, enabling 'test' target")
		add_custom_target(test COMMAND openshot-test)
		list(APPEND OS_TEST_CMDS " or " "'make test'")
	endif()

	string(CONCAT t ${OS_TEST_CMDS})
	message("\nTo run unit tests, use: ${t}")
endif()
