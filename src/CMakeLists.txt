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

################ OPTIONS ##################
# Optional build settings for libopenshot
OPTION(USE_SYSTEM_JSONCPP "Use system installed JsonCpp" OFF)

################ WINDOWS ##################
# Set some compiler options for Windows
# required for libopenshot-audio headers
IF (WIN32)
	add_definitions( -DIGNORE_JUCE_HYPOT=1 )
	SET(CMAKE_CXX_FLAGS " ${CMAKE_CXX_FLAGS} -include cmath")
ENDIF(WIN32)
IF (APPLE)
	# If you still get errors compiling with GCC 4.8, mac headers need to be patched: http://hamelot.co.uk/programming/osx-gcc-dispatch_block_t-has-not-been-declared-invalid-typedef/
	SET_PROPERTY(GLOBAL PROPERTY JUCE_MAC "JUCE_MAC")
	ADD_DEFINITIONS(-DNDEBUG)
	SET(EXTENSION "mm")

	SET(JUCE_PLATFORM_SPECIFIC_DIR build/macosx/platform_specific_code)
	SET(JUCE_PLATFORM_SPECIFIC_LIBRARIES "-framework Carbon -framework Cocoa -framework CoreFoundation -framework CoreAudio -framework CoreMidi -framework IOKit -framework AGL -framework AudioToolbox -framework QuartzCore -lobjc -framework Accelerate")
ENDIF(APPLE)

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

################### FFMPEG #####################
# Find FFmpeg libraries (used for video encoding / decoding)
FIND_PACKAGE(FFmpeg REQUIRED)

IF (AVCODEC_FOUND)
	include_directories(${AVCODEC_INCLUDE_DIRS})
ENDIF (AVCODEC_FOUND)
IF (AVDEVICE_FOUND)
	include_directories(${AVDEVICE_INCLUDE_DIRS})
ENDIF (AVDEVICE_FOUND)
IF (AVFORMAT_FOUND)
	include_directories(${AVFORMAT_INCLUDE_DIRS})
ENDIF (AVFORMAT_FOUND)
IF (AVFILTER_FOUND)
	include_directories(${AVFILTER_INCLUDE_DIRS})
ENDIF (AVFILTER_FOUND)
IF (AVUTIL_FOUND)
	include_directories(${AVUTIL_INCLUDE_DIRS})
ENDIF (AVUTIL_FOUND)
IF (POSTPROC_FOUND)
	include_directories(${POSTPROC_INCLUDE_DIRS})
ENDIF (POSTPROC_FOUND)
IF (SWSCALE_FOUND)
	include_directories(${SWSCALE_INCLUDE_DIRS})
ENDIF (SWSCALE_FOUND)
IF (SWRESAMPLE_FOUND)
	include_directories(${SWRESAMPLE_INCLUDE_DIRS})
ENDIF (SWRESAMPLE_FOUND)
IF (AVRESAMPLE_FOUND)
	include_directories(${AVRESAMPLE_INCLUDE_DIRS})
ENDIF (AVRESAMPLE_FOUND)

################# LIBOPENSHOT-AUDIO ###################
# Find JUCE-based openshot Audio libraries
FIND_PACKAGE(OpenShotAudio REQUIRED)

# Include Juce headers (needed for compile)
include_directories(${LIBOPENSHOT_AUDIO_INCLUDE_DIRS})

################# QT5 ###################
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

################### ZEROMQ #####################
# Find ZeroMQ library (used for socket communication & logging)
FIND_PACKAGE(ZMQ REQUIRED)

# Include ZeroMQ headers (needed for compile)
include_directories(${ZMQ_INCLUDE_DIRS})

################### RESVG #####################
# Find resvg library (used for rendering svg files)
FIND_PACKAGE(RESVG)

# Include resvg headers (optional SVG library)
if (RESVG_FOUND)
   include_directories(${RESVG_INCLUDE_DIRS})

   # define a global var (used in the C++)
   add_definitions( -DUSE_RESVG=1 )
   SET(CMAKE_SWIG_FLAGS "-DUSE_RESVG=1")
endif(RESVG_FOUND)

################### JSONCPP #####################
# Include jsoncpp headers (needed for JSON parsing)
if (USE_SYSTEM_JSONCPP)
	find_package(JsonCpp REQUIRED)
	include_directories(${JSONCPP_INCLUDE_DIRS})
else()
	message("-- Could NOT find JsonCpp library (Using embedded JsonCpp instead)")
	include_directories("../thirdparty/jsoncpp/include")
endif(USE_SYSTEM_JSONCPP)

###############  PROFILING  #################
#set(PROFILER "/usr/lib/libprofiler.so.0.3.2")
#set(PROFILER "/usr/lib/libtcmalloc.so.4")

#### GET LIST OF EFFECT FILES ####
FILE(GLOB EFFECT_FILES "${CMAKE_CURRENT_SOURCE_DIR}/effects/*.cpp")

#### GET LIST OF QT PLAYER FILES ####
FILE(GLOB QT_PLAYER_FILES "${CMAKE_CURRENT_SOURCE_DIR}/Qt/*.cpp")

###############  SET LIBRARY SOURCE FILES  #################
SET ( OPENSHOT_SOURCE_FILES
		AudioBufferSource.cpp
		AudioReaderSource.cpp
		AudioResampler.cpp
		CacheBase.cpp
		CacheDisk.cpp
		CacheMemory.cpp
		ChunkReader.cpp
		ChunkWriter.cpp
		Color.cpp
		Clip.cpp
		ClipBase.cpp
		Coordinate.cpp
		CrashHandler.cpp
		DummyReader.cpp
		ReaderBase.cpp
		RendererBase.cpp
		WriterBase.cpp
		EffectBase.cpp
		${EFFECT_FILES}
		EffectInfo.cpp
		FFmpegReader.cpp
		FFmpegWriter.cpp
		Fraction.cpp
		Frame.cpp
		FrameMapper.cpp
		KeyFrame.cpp
		ZmqLogger.cpp
		PlayerBase.cpp
		Point.cpp
		Profiles.cpp
		QtImageReader.cpp
		QtPlayer.cpp
		Settings.cpp
		Timeline.cpp

		# Qt Video Player
		${QT_PLAYER_FILES}
		${MOC_FILES})

IF (NOT USE_SYSTEM_JSONCPP)
	# Third Party JSON Parser
	SET ( OPENSHOT_SOURCE_FILES ${OPENSHOT_SOURCE_FILES}
			../thirdparty/jsoncpp/src/lib_json/json_reader.cpp
			../thirdparty/jsoncpp/src/lib_json/json_value.cpp
			../thirdparty/jsoncpp/src/lib_json/json_writer.cpp)
ENDIF (NOT USE_SYSTEM_JSONCPP)

# ImageMagic related files
IF (ImageMagick_FOUND)
	SET ( OPENSHOT_SOURCE_FILES ${OPENSHOT_SOURCE_FILES}
			ImageReader.cpp
			ImageWriter.cpp
			TextReader.cpp)
ENDIF (ImageMagick_FOUND)

# BlackMagic related files
IF (BLACKMAGIC_FOUND)
	SET ( OPENSHOT_SOURCE_FILES ${OPENSHOT_SOURCE_FILES}
			DecklinkInput.cpp
			DecklinkReader.cpp
			DecklinkOutput.cpp
			DecklinkWriter.cpp)
ENDIF (BLACKMAGIC_FOUND)


# Get list of headers
file(GLOB_RECURSE headers ${CMAKE_SOURCE_DIR}/include/*.h)

# Disable RPATH
SET(CMAKE_MACOSX_RPATH 0)

############### CREATE LIBRARY #################
# Create shared openshot library
add_library(openshot SHARED
		${OPENSHOT_SOURCE_FILES}
		${headers} )

# Set SONAME and other library properties
set_target_properties(openshot
		PROPERTIES
		VERSION ${PROJECT_VERSION}
		SOVERSION ${SO_VERSION}
		INSTALL_NAME_DIR "${CMAKE_INSTALL_PREFIX}/lib"
		)

###############  LINK LIBRARY  #################
SET ( REQUIRED_LIBRARIES
		${LIBOPENSHOT_AUDIO_LIBRARIES}
		${QT_LIBRARIES}
		${PROFILER}
		${JSONCPP_LIBRARY}
		${ZMQ_LIBRARIES}
		)

IF (AVCODEC_FOUND)
	SET ( REQUIRED_LIBRARIES ${REQUIRED_LIBRARIES} ${AVCODEC_LIBRARIES} )
ENDIF (AVCODEC_FOUND)
IF (AVDEVICE_FOUND)
	SET ( REQUIRED_LIBRARIES ${REQUIRED_LIBRARIES} ${AVDEVICE_LIBRARIES} )
ENDIF (AVDEVICE_FOUND)
IF (AVFORMAT_FOUND)
	SET ( REQUIRED_LIBRARIES ${REQUIRED_LIBRARIES} ${AVFORMAT_LIBRARIES} )
ENDIF (AVFORMAT_FOUND)
IF (AVFILTER_FOUND)
	SET ( REQUIRED_LIBRARIES ${REQUIRED_LIBRARIES} ${AVFILTER_LIBRARIES} )
ENDIF (AVFILTER_FOUND)
IF (AVUTIL_FOUND)
	SET ( REQUIRED_LIBRARIES ${REQUIRED_LIBRARIES} ${AVUTIL_LIBRARIES} )
ENDIF (AVUTIL_FOUND)
IF (POSTPROC_FOUND)
	SET ( REQUIRED_LIBRARIES ${REQUIRED_LIBRARIES} ${POSTPROC_LIBRARIES} )
ENDIF (POSTPROC_FOUND)
IF (SWSCALE_FOUND)
	SET ( REQUIRED_LIBRARIES ${REQUIRED_LIBRARIES} ${SWSCALE_LIBRARIES} )
ENDIF (SWSCALE_FOUND)
IF (SWRESAMPLE_FOUND)
	SET ( REQUIRED_LIBRARIES ${REQUIRED_LIBRARIES} ${SWRESAMPLE_LIBRARIES} )
ENDIF (SWRESAMPLE_FOUND)
IF (AVRESAMPLE_FOUND)
	SET ( REQUIRED_LIBRARIES ${REQUIRED_LIBRARIES} ${AVRESAMPLE_LIBRARIES} )
ENDIF (AVRESAMPLE_FOUND)

IF (RESVG_FOUND)
	SET ( REQUIRED_LIBRARIES ${REQUIRED_LIBRARIES} ${RESVG_LIBRARIES} )
ENDIF(RESVG_FOUND)

IF (OPENMP_FOUND)
	SET ( REQUIRED_LIBRARIES ${REQUIRED_LIBRARIES} ${OpenMP_CXX_FLAGS} )
ENDIF (OPENMP_FOUND)

IF (ImageMagick_FOUND)
	SET ( REQUIRED_LIBRARIES ${REQUIRED_LIBRARIES} ${ImageMagick_LIBRARIES} )
ENDIF (ImageMagick_FOUND)

IF (BLACKMAGIC_FOUND)
	SET ( REQUIRED_LIBRARIES ${REQUIRED_LIBRARIES} ${BLACKMAGIC_LIBRARY_DIR} )
ENDIF (BLACKMAGIC_FOUND)

IF (WIN32)
	# Required for exception handling on Windows
	SET ( REQUIRED_LIBRARIES ${REQUIRED_LIBRARIES} "imagehlp" "dbghelp" )
ENDIF(WIN32)

# Link all referenced libraries
target_link_libraries(openshot ${REQUIRED_LIBRARIES})


############### CLI EXECUTABLE ################
# Create test executable
add_executable(openshot-example examples/Example.cpp)

# Link test executable to the new library
target_link_libraries(openshot-example openshot)

############### PLAYER EXECUTABLE ################
# Create test executable
add_executable(openshot-player Qt/demo/main.cpp)

# Link test executable to the new library
target_link_libraries(openshot-player openshot)

############### TEST BLACKMAGIC CAPTURE APP ################
IF (BLACKMAGIC_FOUND)
	# Create test executable
	add_executable(openshot-blackmagic
			examples/ExampleBlackmagic.cpp)

	# Link test executable to the new library
	target_link_libraries(openshot-blackmagic openshot)
ENDIF (BLACKMAGIC_FOUND)

############### INCLUDE SWIG BINDINGS ################
add_subdirectory(bindings)


############### INSTALL HEADERS & LIBRARY ################
set(LIB_INSTALL_DIR lib${LIB_SUFFIX}) # determine correct lib folder

# Install primary library
INSTALL(  TARGETS openshot
		ARCHIVE DESTINATION ${LIB_INSTALL_DIR}
		LIBRARY DESTINATION ${LIB_INSTALL_DIR}
		COMPONENT library )

INSTALL(DIRECTORY ${CMAKE_SOURCE_DIR}/include/
		DESTINATION ${CMAKE_INSTALL_PREFIX}/include/libopenshot
		FILES_MATCHING PATTERN "*.h")

############### CPACK PACKAGING ##############
IF(MINGW)
	SET(CPACK_GENERATOR "NSIS")
ENDIF(MINGW)
IF(UNIX AND NOT APPLE)
	SET(CPACK_GENERATOR "DEB")
ENDIF(UNIX AND NOT APPLE)
#IF(UNIX AND APPLE)
#	SET(CPACK_GENERATOR "DragNDrop")
#ENDIF(UNIX AND APPLE)
SET(CPACK_DEBIAN_PACKAGE_MAINTAINER "Jonathan Thomas") #required

INCLUDE(CPack)
