##################### src/CMakeLists.txt (libopenshot) #######################
# @brief CMake build file for libopenshot (used to generate makefiles)
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


################ IMAGE MAGICK ##################
# Find the ImageMagick++ library
FIND_PACKAGE(ImageMagick REQUIRED COMPONENTS Magick++ MagickWand MagickCore)

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
include_directories(${LIBJUCE_INCLUDE_DIRS})

################# SDL ###################
# Find SDL libraries
SET(SDL_BUILDING_LIBRARY "True") # This prevents SDL_main from being linked
FIND_PACKAGE(SDL REQUIRED)

# Include SDL headers (needed for compile)
include_directories(${SDL_INCLUDE_DIR})

################# QT5 ###################
# As moc files are generated in the binary dir, tell CMake
# to always look for includes there:
#set(CMAKE_INCLUDE_CURRENT_DIR ON)

#set(CMAKE_AUTOMOC ON)
#set (CMAKE_PREFIX_PATH "/usr/local/Cellar/qt5/5.1.1/")

# Find QT5 libraries
find_package(Qt5Widgets REQUIRED)
find_package(Qt5Core REQUIRED)
find_package(Qt5Gui REQUIRED)
find_package(Qt5Multimedia REQUIRED)
find_package(Qt5MultimediaWidgets REQUIRED)

# Include Qt headers (needed for compile)
include_directories(${Qt5Widgets_INCLUDE_DIRS})
include_directories(${Qt5Core_INCLUDE_DIRS})
include_directories(${Qt5Gui_INCLUDE_DIRS})
include_directories(${Qt5Multimedia_INCLUDE_DIRS})
include_directories(${Qt5MultimediaWidgets_INCLUDE_DIRS})

add_definitions(${Qt5Widgets_DEFINITIONS})
add_definitions(${Qt5Core_DEFINITIONS})
add_definitions(${Qt5Gui_DEFINITIONS})
add_definitions(${Qt5Multimedia_DEFINITIONS})
add_definitions(${Qt5MultimediaWidgets_DEFINITIONS})

SET(QT_LIBRARIES ${Qt5Widgets_LIBRARIES} 
				 ${Qt5Core_LIBRARIES}
				 ${Qt5Gui_LIBRARIES}
				 ${Qt5Multimedia_LIBRARIES}
				 ${Qt5MultimediaWidgets_LIBRARIES})

# Set compiler flags for Qt
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} ${Qt5Widgets_EXECUTABLE_COMPILE_FLAGS} ")
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} ${Qt5Core_EXECUTABLE_COMPILE_FLAGS} ")
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} ${Qt5Gui_EXECUTABLE_COMPILE_FLAGS} ")
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} ${Qt5Multimedia_EXECUTABLE_COMPILE_FLAGS} ")
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} ${Qt5MultimediaWidgets_EXECUTABLE_COMPILE_FLAGS} ")

# Manually moc Qt files
qt5_wrap_cpp(MOC_FILES ${QT_HEADER_FILES})

################# BLACKMAGIC DECKLINK ###################
# Find BlackMagic DeckLinkAPI libraries
FIND_PACKAGE(BlackMagic)

IF (BLACKMAGIC_FOUND)
	# Include headers (needed for compile)
	include_directories(${BLACKMAGIC_INCLUDE_DIR})
	
	# define a global var (used in the C++)
	add_definitions( -DUSE_BLACKMAGIC=1 )
	SET(CMAKE_SWIG_FLAGS "-DUSE_BLACKMAGIC=1")
	
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

###############  PROFILING  #################
#set(PROFILER "/usr/lib/libprofiler.so")
#set(PROFILER "/usr/lib/libtcmalloc.so")

#### GET LIST OF EFFECT FILES ####
FILE(GLOB EFFECT_FILES "${CMAKE_CURRENT_SOURCE_DIR}/effects/*.cpp")

#### GET LIST OF QT PLAYER FILES ####
FILE(GLOB QT_PLAYER_FILES "${CMAKE_CURRENT_SOURCE_DIR}/Qt/*.cpp")

###############  SET LIBRARY SOURCE FILES  #################
SET ( OPENSHOT_SOURCE_FILES
  		AudioBufferSource.cpp
		AudioResampler.cpp
		Cache.cpp
		ChunkReader.cpp
		ChunkWriter.cpp
		Color.cpp
		Clip.cpp
		ClipBase.cpp
		Coordinate.cpp
		DummyReader.cpp
		ReaderBase.cpp
		WriterBase.cpp
		EffectBase.cpp
		${EFFECT_FILES}
		FFmpegReader.cpp
		FFmpegWriter.cpp
		Fraction.cpp
		Frame.cpp
		FrameMapper.cpp
		KeyFrame.cpp
		ImageReader.cpp
		PlayerBase.cpp
		Point.cpp
		Profile.cpp
		SDLPlayer.cpp
		TextReader.cpp
		Timeline.cpp
		
		# Qt Video Player
		${QT_PLAYER_FILES}
		${MOC_FILES}
		
		# Third Party JSON Parser
		../thirdparty/jsoncpp/src/lib_json/json_reader.cpp
		../thirdparty/jsoncpp/src/lib_json/json_value.cpp
		../thirdparty/jsoncpp/src/lib_json/json_writer.cpp )
		
		IF (BLACKMAGIC_FOUND)
			SET ( OPENSHOT_SOURCE_FILES ${OPENSHOT_SOURCE_FILES} 
				DecklinkInput.cpp
				DecklinkReader.cpp
				DecklinkOutput.cpp
				DecklinkWriter.cpp)
		ENDIF (BLACKMAGIC_FOUND)
		

############### CREATE LIBRARY #################
# Create shared openshot library
add_library(openshot SHARED
			${OPENSHOT_SOURCE_FILES})
			
# Set SONAME and other library properties
set_target_properties(openshot
			PROPERTIES
			VERSION ${PROJECT_VERSION}
			SOVERSION ${SO_VERSION})

###############  LINK LIBRARY  #################
SET ( REQUIRED_LIBRARIES
  		${ImageMagick_LIBRARIES}
		${FFMPEG_LIBRARIES}
		${LIBJUCE_LIBRARIES}
		${SDL_LIBRARY}
		${QT_LIBRARIES}
		${PROFILER}
		)
		
		IF (OPENMP_FOUND)
			SET ( REQUIRED_LIBRARIES ${REQUIRED_LIBRARIES} ${OpenMP_CXX_FLAGS} )
		ENDIF (OPENMP_FOUND)
		
		IF (BLACKMAGIC_FOUND)
			SET ( REQUIRED_LIBRARIES ${REQUIRED_LIBRARIES} ${BLACKMAGIC_LIBRARY_DIR} )
		ENDIF (BLACKMAGIC_FOUND)

# Link all referenced libraries
target_link_libraries(openshot ${REQUIRED_LIBRARIES})


############### TEST EXECUTABLE ################
# Create test executable
add_executable(example Main.cpp)

# Link test executable to the new library
target_link_libraries(example openshot)

############### TEST QT EXECUTABLE ################
# Create test executable
add_executable(qt_player Qt/main_player.cpp)

# Link test executable to the new library
target_link_libraries(qt_player openshot)

############### TEST BLACKMAGIC CAPTURE APP ################
IF (BLACKMAGIC_FOUND)
	# Create test executable
	add_executable(blackmagic 
				   Main_Blackmagic.cpp)
	
	# Link test executable to the new library
	target_link_libraries(blackmagic openshot)
ENDIF (BLACKMAGIC_FOUND)

############### SWIG PYTHON BINDINGS ################
FIND_PACKAGE(SWIG 2.0 REQUIRED)
INCLUDE(${SWIG_USE_FILE})

FIND_PACKAGE(PythonLibs)
INCLUDE_DIRECTORIES(${PYTHON_INCLUDE_PATH})
INCLUDE_DIRECTORIES(${CMAKE_CURRENT_SOURCE_DIR})

SET_SOURCE_FILES_PROPERTIES(openshot.i PROPERTIES CPLUSPLUS ON)
SWIG_ADD_MODULE(openshot python openshot.i)
SWIG_LINK_LIBRARIES(openshot ${PYTHON_LIBRARIES} openshot)

############### INSTALL HEADERS & LIBRARY ################
# Headers
file(GLOB_RECURSE headers ${CMAKE_SOURCE_DIR}/include/*.h)
INSTALL(FILES ${headers} DESTINATION include/libopenshot)

# Install primary library
INSTALL(TARGETS openshot DESTINATION lib)

# Install Swig python library
INSTALL(TARGETS _openshot DESTINATION share/pyshared/libopenshot)

# Install Python bindings
INSTALL(FILES ${CMAKE_CURRENT_BINARY_DIR}/openshot.py DESTINATION share/pyshared/libopenshot)
INSTALL(FILES ${CMAKE_SOURCE_DIR}/cmake/Python/__init__.py DESTINATION share/pyshared/libopenshot)
