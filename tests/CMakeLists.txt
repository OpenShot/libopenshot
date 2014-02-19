##################### tests/CMakeLists.txt (libopenshot) ######################
# @brief CMake build file for libopenshot unit tests (used to generate makefiles)
# @author Jonathan Thomas <jonathan@openshot.org>
#
# @section LICENSE
#
# Copyright (c) 2008-2013 OpenShot Studios, LLC
# (http://www.openshotstudios.com). This file is part of
# OpenShot Library (http://www.openshot.org), an open-source project
# dedicated to delivering high quality video editing and animation solutions
# to the world.
#
# OpenShot Library is free software: you can redistribute it and/or modify
# it under the terms of the GNU General Public License as published by
# the Free Software Foundation, either version 3 of the License, or
# (at your option) any later version.
#
# OpenShot Library is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
# GNU General Public License for more details.
#
# You should have received a copy of the GNU General Public License
# along with OpenShot Library.  If not, see <http://www.gnu.org/licenses/>.
################################################################################


################### UNITTEST++ #####################
# Find UnitTest++ libraries (used for unit testing)
FIND_PACKAGE(UnitTest++ REQUIRED)

# Include UnitTest++ headers (needed for compile)
include_directories(${UNITTEST++_INCLUDE_DIR})

################ IMAGE MAGICK ##################
# Find the ImageMagick++ library
FIND_PACKAGE(ImageMagick COMPONENTS Magick++ MagickWand MagickCore)

# Include ImageMagick++ headers (needed for compile)
include_directories(${ImageMagick_INCLUDE_DIRS})

################### FFMPEG #####################
# Find FFmpeg libraries (used for video encoding / decoding)
FIND_PACKAGE(FFmpeg REQUIRED)

# Include FFmpeg headers (needed for compile)
include_directories(${FFMPEG_INCLUDE_DIR})

################# LIBOPENSHOT-AUDIO ###################
# Find JUCE-based openshot Audio libraries
FIND_PACKAGE(OpenShotAudio REQUIRED)

# Include Juce headers (needed for compile)
include_directories(${LIBOPENSHOT_AUDIO_INCLUDE_DIRS})

################# SDL ###################
# Find SDL libraries
FIND_PACKAGE(SDL REQUIRED)

# Include SDL headers (needed for compile)
include_directories(${SDL_INCLUDE_DIR})

################# QT5 ###################
set(CMAKE_AUTOMOC ON)

# Find QT5 libraries
find_package(Qt5Widgets)
find_package(Qt5Core)
find_package(Qt5Gui)
find_package(Qt5Multimedia)

# Include Qt headers (needed for compile)
include_directories(${Qt5Widgets_INCLUDE_DIRS})
include_directories(${Qt5Core_INCLUDE_DIRS})
include_directories(${Qt5Gui_INCLUDE_DIRS})
include_directories(${Qt5Multimedia_INCLUDE_DIRS})

add_definitions(${Qt5Widgets_DEFINITIONS})
add_definitions(${Qt5Core_DEFINITIONS})
add_definitions(${Qt5Gui_DEFINITIONS})
add_definitions(${Qt5Multimedia_DEFINITIONS})

# Add Multimedia library
SET(QT_LIBRARIES ${Qt5Widgets_LIBRARIES} 
				 ${Qt5Core_LIBRARIES}
				 ${Qt5Gui_LIBRARIES}
				 ${Qt5Multimedia_LIBRARIES})

# Set compiler flags for Qt
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} ${Qt5Widgets_EXECUTABLE_COMPILE_FLAGS} ")
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} ${Qt5Core_EXECUTABLE_COMPILE_FLAGS} ")
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} ${Qt5Gui_EXECUTABLE_COMPILE_FLAGS} ")
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} ${Qt5Multimedia_EXECUTABLE_COMPILE_FLAGS} ")

################# BLACKMAGIC DECKLINK ###################
# Find BlackMagic DeckLinkAPI libraries
FIND_PACKAGE(BlackMagic)

IF (BLACKMAGIC_FOUND)
	# Include SDL headers (needed for compile)
	include_directories(${BLACKMAGIC_INCLUDE_DIR})
ENDIF (BLACKMAGIC_FOUND)

################### OPENMP #####################
# Check for OpenMP (used for multi-core processing)
FIND_PACKAGE(OpenMP)

if (OPENMP_FOUND)
   set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} ${OpenMP_CXX_FLAGS} ")
endif(OPENMP_FOUND)

################### JSONCPP #####################
# Include jsoncpp headers (needed for JSON parsing)
include_directories("../thirdparty/jsoncpp/include")

################ TESTER EXECUTABLE #################
# Create unit test executable (tester)
add_executable(tester
			   tests.cpp
			   Cache_Tests.cpp
			   Clip_Tests.cpp
			   Coordinate_Tests.cpp
			   ReaderBase_Tests.cpp
			   FFmpegReader_Tests.cpp
			   Fraction_Tests.cpp
			   FrameMapper_Tests.cpp
			   KeyFrame_Tests.cpp
			   Point_Tests.cpp
			   Timeline_Tests.cpp)

# Link libraries to the new executable
target_link_libraries(tester openshot ${UNITTEST++_LIBRARY})


#################### MAKE TEST ######################
# Hook up the 'make test' target to the 'tester' executable
ADD_CUSTOM_TARGET(test ${CMAKE_CURRENT_BINARY_DIR}/tester)