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
# and/or modify it under the terms of the GNU Affero General Public License
# as published by the Free Software Foundation, either version 3 of the
# License, or (at your option) any later version.
#
# OpenShot Library (libopenshot) is distributed in the hope that it will be
# useful, but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
# GNU Affero General Public License for more details.
#
# You should have received a copy of the GNU Affero General Public License
# along with OpenShot Library. If not, see <http://www.gnu.org/licenses/>.
#
# Also, if your software can interact with users remotely through a computer
# network, you should also make sure that it provides a way for users to
# get its source. For example, if your program is a web application, its
# interface could display a "Source" link that leads users to an archive
# of the code. There are many ways you could offer source, and different
# solutions will be better for different programs; see section 13 for the
# specific requirements.
#
# You should also get your employer (if you work as a programmer) or school,
# if any, to sign a "copyright disclaimer" for the program, if necessary.
# For more information on this, and how to apply and follow the GNU AGPL, see
# <http://www.gnu.org/licenses/>.
################################################################################


################ IMAGE MAGICK ##################
# Find the ImageMagick++ library
FIND_PACKAGE(ImageMagick REQUIRED COMPONENTS Magick++ MagickWand MagickCore)

# Include ImageMagick++ headers (needed for compile)
include_directories(${ImageMagick_INCLUDE_DIRS})

# Set the Quantum Depth that ImageMagick was built with (default to 16 bits)
IF (MAGICKCORE_QUANTUM_DEPTH)
	add_definitions( -DMAGICKCORE_QUANTUM_DEPTH=${MAGICKCORE_QUANTUM_DEPTH} )
ENDIF (MAGICKCORE_QUANTUM_DEPTH)
IF (MAGICKCORE_HDRI_ENABLE)
	add_definitions( -DMAGICKCORE_HDRI_ENABLE=${MAGICKCORE_HDRI_ENABLE} )
ENDIF (MAGICKCORE_HDRI_ENABLE)
	
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
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -g -ggdb ")

# Manually moc Qt files
qt5_wrap_cpp(MOC_FILES ${QT_HEADER_FILES})

################# BLACKMAGIC DECKLINK ###################
# Find BlackMagic DeckLinkAPI libraries
IF (ENABLE_BLACKMAGIC)
	FIND_PACKAGE(BlackMagic)
	
	IF (BLACKMAGIC_FOUND)
		# Include headers (needed for compile)
		include_directories(${BLACKMAGIC_INCLUDE_DIR})
		
		# define a global var (used in the C++)
		add_definitions( -DUSE_BLACKMAGIC=1 )
		SET(CMAKE_SWIG_FLAGS "-DUSE_BLACKMAGIC=1")
		
	ENDIF (BLACKMAGIC_FOUND)
ENDIF (ENABLE_BLACKMAGIC)

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
  		AudioReaderSource.cpp
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
		RendererBase.cpp
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
		Profiles.cpp
		QtPlayer.cpp
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
		${LIBOPENSHOT_AUDIO_LIBRARIES}
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
add_executable(demo_player Qt/demo/main.cpp)

# Link test executable to the new library
target_link_libraries(demo_player openshot)

############### TEST BLACKMAGIC CAPTURE APP ################
IF (BLACKMAGIC_FOUND)
	# Create test executable
	add_executable(blackmagic 
				   Main_Blackmagic.cpp)
	
	# Link test executable to the new library
	target_link_libraries(blackmagic openshot)
ENDIF (BLACKMAGIC_FOUND)

############### INCLUDE SWIG BINDINGS ################
add_subdirectory(bindings)


############### INSTALL HEADERS & LIBRARY ################
set(LIB_INSTALL_DIR lib${LIB_SUFFIX}) # determine correct lib folder

# Headers
file(GLOB_RECURSE headers ${CMAKE_SOURCE_DIR}/include/*.h)
INSTALL(FILES ${headers} DESTINATION include/libopenshot)

# Install primary library
INSTALL(TARGETS openshot DESTINATION ${LIB_INSTALL_DIR})


############### CPACK PACKAGING ##############
IF(MINGW)
	SET(CPACK_GENERATOR "NSIS")
ENDIF(MINGW)
IF(UNIX AND NOT APPLE)
	SET(CPACK_GENERATOR "DEB")
ENDIF(UNIX AND NOT APPLE)
IF(UNIX AND APPLE)
	SET(CPACK_GENERATOR "DragNDrop")
ENDIF(UNIX AND APPLE)
SET(CPACK_DEBIAN_PACKAGE_MAINTAINER "Jonathan Thomas") #required

INCLUDE(CPack)
