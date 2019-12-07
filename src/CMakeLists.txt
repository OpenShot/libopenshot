####################### CMakeLists.txt (libopenshot) #########################
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

# Collect and display summary of options/dependencies
include(FeatureSummary)

################ OPTIONS ##################
# Optional build settings for libopenshot
option(USE_SYSTEM_JSONCPP "Use system installed JsonCpp, if found" ON)
option(DISABLE_BUNDLED_JSONCPP "Don't fall back to bundled JsonCpp" OFF)
option(ENABLE_IWYU "Enable 'Include What You Use' scanner (CMake 3.3+)" OFF)

# Automatically process Qt classes with meta-object compiler
set(CMAKE_AUTOMOC True)

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

################# LIBOPENSHOT-AUDIO ###################
# Find JUCE-based openshot Audio libraries
FIND_PACKAGE(OpenShotAudio 0.1.8 REQUIRED)

# Include Juce headers (needed for compile)
include_directories(${LIBOPENSHOT_AUDIO_INCLUDE_DIRS})

################# BLACKMAGIC DECKLINK ###################
# Find BlackMagic DeckLinkAPI libraries
IF (ENABLE_BLACKMAGIC)
	FIND_PACKAGE(BlackMagic)

	IF (BLACKMAGIC_FOUND)
		# Include Blackmagic headers (needed for compile)
		include_directories(${BLACKMAGIC_INCLUDE_DIR})

		# define a global var (used in the C++)
		add_definitions( -DUSE_BLACKMAGIC=1 )
		SET(CMAKE_SWIG_FLAGS "-DUSE_BLACKMAGIC=1")

	ENDIF (BLACKMAGIC_FOUND)
ENDIF (ENABLE_BLACKMAGIC)

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

###############  PROFILING  #################
#set(PROFILER "/usr/lib/libprofiler.so.0.3.2")
#set(PROFILER "/usr/lib/libtcmalloc.so.4")

if(CMAKE_VERSION VERSION_LESS 3.3)
  # IWYU wasn't supported internally in 3.2
  set(ENABLE_IWYU FALSE)
endif()

if(ENABLE_IWYU)
	find_program(IWYU_PATH NAMES "iwyu"
		DOC "include-what-you-use source code scanner executable")
	if(IWYU_PATH)
		if(IWYU_OPTS)
			separate_arguments(IWYU_OPTS)
			list(APPEND _iwyu_opts "-Xiwyu" ${IWYU_OPTS})
		endif()
		set(CMAKE_CXX_INCLUDE_WHAT_YOU_USE ${IWYU_PATH} ${_iwyu_opts})
	else()
		set(ENABLE_IWYU FALSE)
	endif()
endif()
add_feature_info("IWYU (include-what-you-use)" ENABLE_IWYU "Scan all source files with 'iwyu'")

# Main library sources
set(OPENSHOT_SOURCES
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
  EffectInfo.cpp
  FFmpegReader.cpp
  FFmpegWriter.cpp
  Fraction.cpp
  Frame.cpp
  FrameMapper.cpp
  KeyFrame.cpp
  OpenShotVersion.cpp
  ZmqLogger.cpp
  PlayerBase.cpp
  Point.cpp
  Profiles.cpp
  QtHtmlReader.cpp
  QtImageReader.cpp
  QtPlayer.cpp
  QtTextReader.cpp
  Settings.cpp
  Timeline.cpp)

# Video effects
set(EFFECTS_SOURCES
  effects/Bars.cpp
  effects/Blur.cpp
  effects/Brightness.cpp
  effects/ChromaKey.cpp
  effects/ColorShift.cpp
  effects/Crop.cpp
  effects/Deinterlace.cpp
  effects/Hue.cpp
  effects/Mask.cpp
  effects/Negate.cpp
  effects/Pixelate.cpp
  effects/Saturation.cpp
  effects/Shift.cpp
  effects/Wave.cpp)

# Qt video player components
set(QT_PLAYER_SOURCES
  Qt/AudioPlaybackThread.cpp
  Qt/PlayerDemo.cpp
  Qt/PlayerPrivate.cpp
  Qt/VideoCacheThread.cpp
  Qt/VideoPlaybackThread.cpp
  Qt/VideoRenderer.cpp
  Qt/VideoRenderWidget.cpp)


# Get list of headers
file(GLOB_RECURSE headers ${CMAKE_SOURCE_DIR}/include/*.h)

# Disable RPATH
SET(CMAKE_MACOSX_RPATH 0)

############### CREATE LIBRARY #################
# Create shared openshot library
add_library(openshot SHARED)

target_sources(openshot
  PRIVATE
    ${OPENSHOT_SOURCES} ${EFFECTS_SOURCES} ${QT_PLAYER_SOURCES}
  PUBLIC
    ${headers})

# Set SONAME and other library properties
set_target_properties(openshot
		PROPERTIES
		VERSION ${PROJECT_VERSION}
		SOVERSION ${PROJECT_SO_VERSION}
		INSTALL_NAME_DIR "${CMAKE_INSTALL_PREFIX}/lib"
		)

# Add optional ImageMagic-dependent sources
if(ImageMagick_FOUND)
	target_sources(openshot PRIVATE
    ImageReader.cpp
    ImageWriter.cpp
    TextReader.cpp)
endif()

# BlackMagic related files
if(BLACKMAGIC_FOUND)
  target_sources(openshot PRIVATE
    DecklinkInput.cpp
    DecklinkReader.cpp
    DecklinkOutput.cpp
    DecklinkWriter.cpp)
endif()

# Location of our includes, both internally and when installed
target_include_directories(openshot
  PRIVATE
    ${CMAKE_SOURCE_DIR}/include
    ${CMAKE_BINARY_DIR}/include
  PUBLIC
    $<BUILD_INTERFACE:${CMAKE_SOURCE_DIR}/include>
    $<BUILD_INTERFACE:${CMAKE_BINARY_DIR}/include>
    $<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}/libopenshot>)


################### JSONCPP #####################
# Include jsoncpp headers (needed for JSON parsing)
if (USE_SYSTEM_JSONCPP)
	message(STATUS "Looking for system JsonCpp")
	find_package(JsonCpp)
	if (JSONCPP_FOUND AND NOT TARGET jsoncpp_lib)
		# Create the expected target, for older installs that don't
		add_library(jsoncpp_lib INTERFACE)
		target_include_directories(jsoncpp_lib INTERFACE
			${JSONCPP_INCLUDE_DIRS})
		target_link_libraries(jsoncpp_lib INTERFACE ${JSONCPP_LIBRARY})
	endif ()
endif ()

if (NOT JSONCPP_FOUND AND NOT DISABLE_BUNDLED_JSONCPP)
  message(STATUS "Using embedded JsonCpp (not found or USE_SYSTEM_JSONCPP disabled)")
  if (NOT TARGET jsoncpp_lib)
    add_library(jsoncpp_lib INTERFACE)
    target_include_directories(jsoncpp_lib INTERFACE
      "${PROJECT_SOURCE_DIR}/thirdparty/jsoncpp")
    target_sources(jsoncpp_lib INTERFACE "${PROJECT_SOURCE_DIR}/thirdparty/jsoncpp/jsoncpp.cpp")
    # Because this satisfies the requirement, an installed JsonCpp is optional
    set_package_properties(JsonCpp PROPERTIES TYPE OPTIONAL)
  endif ()
  add_feature_info("JsonCpp (embedded)" TRUE "JsonCpp will be compiled from the bundled sources")
endif ()

if (JSONCPP_FOUND)
  # JsonCpp is actually required, even though we probe for it optionally
  # (This tells feature_summary() to bail if it's not found, later)
  set_package_properties(JsonCpp PROPERTIES TYPE REQUIRED)
endif ()

# If we found any usable JsonCpp, use it. Otherwise, bail.
if (TARGET jsoncpp_lib)
  target_link_libraries(openshot PUBLIC jsoncpp_lib)
endif ()

################# QT5 ###################
# Find QT5 libraries
set(_qt_components Widgets Core Gui Multimedia MultimediaWidgets)
find_package(Qt5 COMPONENTS ${_qt_components} REQUIRED)

foreach(_qt_comp IN LISTS _qt_components)
  if(TARGET Qt5::${_qt_comp})
    target_link_libraries(openshot PUBLIC Qt5::${_qt_comp})
  endif()
endforeach()

################### FFMPEG #####################
# Find FFmpeg libraries (used for video encoding / decoding)
FIND_PACKAGE(FFmpeg REQUIRED COMPONENTS avcodec avdevice avformat avutil swscale)

foreach(ff_comp avcodec avdevice avformat avfilter avutil postproc swscale swresample avresample)
  if(TARGET FFmpeg::${ff_comp})
		target_link_libraries(openshot PUBLIC FFmpeg::${ff_comp})
  endif()
endforeach()

################### Threads ####################
# Threading library -- uses IMPORTED target Threads::Threads (since CMake 3.1)
set(CMAKE_THREAD_PREFER_PTHREAD TRUE)
set(THREADS_PREFER_PTHREAD_FLAG TRUE)
find_package(Threads REQUIRED)
target_link_libraries(openshot PUBLIC Threads::Threads)

################### OPENMP #####################
# Check for OpenMP (used for multi-core processing)

# OpenMP is required by FFmpegReader/Writer
find_package(OpenMP REQUIRED)

if(NOT TARGET OpenMP::OpenMP_CXX)
    # Older CMake versions (< 3.9) don't create find targets.
    add_library(OpenMP_TARGET INTERFACE)
    add_library(OpenMP::OpenMP_CXX ALIAS OpenMP_TARGET)
    target_compile_options(OpenMP_TARGET INTERFACE ${OpenMP_CXX_FLAGS})
    target_link_libraries(OpenMP_TARGET INTERFACE Threads::Threads)
    target_link_libraries(OpenMP_TARGET INTERFACE ${OpenMP_CXX_FLAGS})
endif()

target_link_libraries(openshot PUBLIC OpenMP::OpenMP_CXX)

################### ZEROMQ #####################
# Find ZeroMQ library (used for socket communication & logging)
find_package(ZeroMQ REQUIRED) # Creates libzmq target

# Some platforms package the header-only cppzmq C++ bindings separately,
# others (Ubuntu) bundle them in with libzmq itself
find_package(cppzmq QUIET) # Creates cppzmq target

# Link ZeroMQ library
if (TARGET libzmq)
	target_link_libraries(openshot PUBLIC libzmq)
endif()
# Include cppzmq headers, if not bundled into libzmq
if (TARGET cppzmq)
  target_link_libraries(openshot PUBLIC cppzmq)
endif()


###############  LINK LIBRARY  #################
# Link remaining dependency libraries
target_link_libraries(openshot PUBLIC
	${LIBOPENSHOT_AUDIO_LIBRARIES}
  ${PROFILER})

if(ImageMagick_FOUND)
  target_link_libraries(openshot PUBLIC ${ImageMagick_LIBRARIES})
endif()

if(RESVG_FOUND)
  target_link_libraries(openshot PUBLIC ${RESVG_LIBRARIES})
endif()

if(BLACKMAGIC_FOUND)
  target_link_libraries(openshot PUBLIC ${BLACKMAGIC_LIBRARY_DIR})
endif()

if(WIN32)
	# Required for exception handling on Windows
	target_link_libraries(openshot PUBLIC "imagehlp" "dbghelp" )
endif()


############### CLI EXECUTABLES ################
# Create test executable
add_executable(openshot-example examples/Example.cpp)

# Define path to test input files
SET(TEST_MEDIA_PATH "${PROJECT_SOURCE_DIR}/src/examples/")
IF (WIN32)
        STRING(REPLACE "/" "\\\\" TEST_MEDIA_PATH TEST_MEDIA_PATH)
ENDIF(WIN32)
target_compile_definitions(openshot-example PRIVATE
	-DTEST_MEDIA_PATH="${TEST_MEDIA_PATH}" )

# Link test executable to the new library
target_link_libraries(openshot-example openshot)

add_executable(openshot-html-test examples/ExampleHtml.cpp)
target_link_libraries(openshot-html-test openshot Qt5::Gui)

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

########### PRINT FEATURE SUMMARY ##############
feature_summary(WHAT ALL
    INCLUDE_QUIET_PACKAGES
    FATAL_ON_MISSING_REQUIRED_PACKAGES
    DESCRIPTION "Displaying feature summary\n\nBuild configuration:")

############### INSTALL HEADERS & LIBRARY ################
set(LIB_INSTALL_DIR lib${LIB_SUFFIX}) # determine correct lib folder

# Install primary library
INSTALL(TARGETS openshot
		ARCHIVE DESTINATION ${LIB_INSTALL_DIR}
		LIBRARY DESTINATION ${LIB_INSTALL_DIR}
		RUNTIME DESTINATION ${LIB_INSTALL_DIR}
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
