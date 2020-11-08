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

include(GNUInstallDirs)

# Enable IN_LIST in older CMake
if (POLICY CMP0057)
  cmake_policy(SET CMP0057 NEW)
endif()

###############  PROFILING  #################
#set(PROFILER "/usr/lib//usr/lib/libprofiler.so.0.4.5")
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
  Json.cpp
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
  TimelineBase.cpp
  Timeline.cpp)

# Video effects
set(EFFECTS_SOURCES
  effects/Bars.cpp
  effects/Blur.cpp
  effects/Brightness.cpp
  effects/Caption.cpp
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

# Disable RPATH
set(CMAKE_MACOSX_RPATH 0)

############### CREATE LIBRARY #################
# Create shared openshot library
add_library(openshot SHARED)

target_sources(openshot PRIVATE
  ${OPENSHOT_SOURCES}
  ${EFFECTS_SOURCES}
  ${QT_PLAYER_SOURCES}
  )

# Set SONAME and other library properties
set_target_properties(openshot PROPERTIES
  AUTOMOC ON
  VERSION ${PROJECT_VERSION}
  SOVERSION ${PROJECT_SO_VERSION}
  INSTALL_NAME_DIR "${CMAKE_INSTALL_PREFIX}/lib"
)

# Location of our includes, both internally and when installed
target_include_directories(openshot
  PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${CMAKE_CURRENT_BINARY_DIR}
  PUBLIC
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}>
    $<BUILD_INTERFACE:${CMAKE_CURRENT_BINARY_DIR}>
    $<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}/libopenshot>)

################# LIBOPENSHOT-AUDIO ###################
# Find JUCE-based openshot Audio libraries
if(NOT TARGET OpenShot::Audio)
  # Only load if necessary (not for integrated builds)
  find_package(OpenShotAudio 0.2.0 REQUIRED)
endif()
target_link_libraries(openshot PUBLIC OpenShot::Audio)

###
### ImageMagick
###

# Find the ImageMagick++ library
find_package(ImageMagick COMPONENTS Magick++ MagickWand MagickCore)

if(ImageMagick_FOUND)
  if(NOT TARGET ImageMagick::Magick++ AND NOT TARGET Magick++_TARGET)
    add_library(Magick++_TARGET INTERFACE)

    # Include ImageMagick++ headers (needed for compile)
    set_property(TARGET Magick++_TARGET APPEND PROPERTY
      INTERFACE_INCLUDE_DIRECTORIES ${ImageMagick_INCLUDE_DIRS})

    # Set the Quantum Depth that ImageMagick was built with (default to 16 bits)
    if(NOT DEFINED MAGICKCORE_QUANTUM_DEPTH)
      set(MAGICKCORE_QUANTUM_DEPTH 16)
    endif()
    if(NOT DEFINED MAGICKCORE_HDRI_ENABLE)
      set(MAGICKCORE_HDRI_ENABLE 0)
    endif()

    set_property(TARGET Magick++_TARGET APPEND PROPERTY
      INTERFACE_COMPILE_DEFINITIONS
        MAGICKCORE_QUANTUM_DEPTH=${MAGICKCORE_QUANTUM_DEPTH})
    set_property(TARGET Magick++_TARGET APPEND PROPERTY
      INTERFACE_COMPILE_DEFINITIONS
        MAGICKCORE_HDRI_ENABLE=${MAGICKCORE_HDRI_ENABLE})

    target_link_libraries(Magick++_TARGET INTERFACE
      ${ImageMagick_LIBRARIES})

    # Alias to our namespaced name
    add_library(ImageMagick::Magick++ ALIAS Magick++_TARGET)

  endif()

  # Add optional ImageMagic-dependent sources
  target_sources(openshot PRIVATE
    ImageReader.cpp
    ImageWriter.cpp
    TextReader.cpp)

  # define a preprocessor macro (used in the C++ source)
  target_compile_definitions(openshot PUBLIC USE_IMAGEMAGICK=1)
  list(APPEND CMAKE_SWIG_FLAGS -DUSE_IMAGEMAGICK=1)

  # Link with ImageMagick library
  target_link_libraries(openshot PUBLIC ImageMagick::Magick++)

  set(HAVE_IMAGEMAGICK TRUE CACHE BOOL "Building with ImageMagick support" FORCE)
  mark_as_advanced(HAVE_IMAGEMAGICK)
endif()


################### JSONCPP #####################
# Include jsoncpp headers (needed for JSON parsing)
if (USE_SYSTEM_JSONCPP)
  message(STATUS "Looking for system jsoncpp")
  # Either an installed config or our find module will
  # create the IMPORTED target jsoncpp_lib if successful
  find_package(jsoncpp)
endif ()

if (NOT jsoncpp_FOUND AND NOT DISABLE_BUNDLED_JSONCPP)
  message(STATUS "Using embedded jsoncpp (not found or USE_SYSTEM_JSONCPP disabled)")
  if (NOT TARGET jsoncpp_lib)
    add_library(jsoncpp_lib INTERFACE)
    target_include_directories(jsoncpp_lib INTERFACE
      "${PROJECT_SOURCE_DIR}/thirdparty/jsoncpp")
    target_sources(jsoncpp_lib INTERFACE "${PROJECT_SOURCE_DIR}/thirdparty/jsoncpp/jsoncpp.cpp")
    # Because this satisfies the requirement, an installed JsonCpp is optional
    set_package_properties(jsoncpp PROPERTIES TYPE OPTIONAL)
  endif ()
  add_feature_info("jsoncpp (embedded)" TRUE "jsoncpp will be compiled from the bundled sources")
endif ()

if (jsoncpp_FOUND)
  # JsonCpp is actually required, even though we probe for it optionally
  # (This tells feature_summary() to bail if it's not found, later)
  set_package_properties(jsoncpp PROPERTIES TYPE REQUIRED)
endif ()

# If we found any usable JsonCpp, use it. Otherwise, bail.
if (TARGET jsoncpp_lib)
  target_link_libraries(openshot PUBLIC jsoncpp_lib)
endif ()

################# QT5 ###################
# Find QT5 libraries
set(_qt_components Core Gui Widgets)
find_package(Qt5 COMPONENTS ${_qt_components} REQUIRED)

foreach(_qt_comp IN LISTS _qt_components)
  if(TARGET Qt5::${_qt_comp})
    target_link_libraries(openshot PUBLIC Qt5::${_qt_comp})
  endif()
endforeach()

# Keep track of Qt version, to embed in our version header
set(QT_VERSION_STR ${Qt5Core_VERSION_STRING} CACHE STRING "Qt version linked with" FORCE)
mark_as_advanced(QT_VERSION_STR)

################### FFMPEG #####################
# Find FFmpeg libraries (used for video encoding / decoding)
find_package(FFmpeg REQUIRED COMPONENTS avcodec avdevice avformat avutil swscale)

set(all_comps avcodec avdevice avformat avfilter avutil postproc swscale swresample avresample)
set(version_comps avcodec avformat avutil)

foreach(ff_comp IN LISTS all_comps)
  if(TARGET FFmpeg::${ff_comp})
    target_link_libraries(openshot PUBLIC FFmpeg::${ff_comp})
    # Keep track of some FFmpeg lib versions, to embed in our version header
    if(${ff_comp} IN_LIST version_comps)
      string(TOUPPER ${ff_comp} v_name)
      set(${v_name}_VERSION_STR ${${ff_comp}_VERSION} CACHE STRING "${ff_comp} version used" FORCE)
      mark_as_advanced(${v_name}_VERSION_STR)
    endif()
  endif()

endforeach()

################### OPENMP #####################
# Check for OpenMP (used for multi-core processing)

# OpenMP is required by FFmpegReader/Writer
find_package(OpenMP REQUIRED)

if(NOT TARGET OpenMP::OpenMP_CXX)
    # Older CMake versions (< 3.9) don't create find targets.
    add_library(OpenMP_TARGET INTERFACE)
    add_library(OpenMP::OpenMP_CXX ALIAS OpenMP_TARGET)
    target_compile_options(OpenMP_TARGET INTERFACE ${OpenMP_CXX_FLAGS})
    target_link_libraries(OpenMP_TARGET INTERFACE ${OpenMP_CXX_FLAGS})
endif()

target_link_libraries(openshot PUBLIC OpenMP::OpenMP_CXX)

###
### ZeroMQ
###

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

###
### Resvg, the Rust SVG library
###

# Migrate some legacy variable names
if(DEFINED RESVGDIR AND NOT DEFINED Resvg_ROOT)
  set(Resvg_ROOT ${RESVGDIR})
endif()
if(DEFINED ENV{RESVGDIR} AND NOT DEFINED Resvg_ROOT)
  set(Resvg_ROOT $ENV{RESVGDIR})
endif()

# Find resvg library (used for rendering svg files)
find_package(Resvg)

# Include resvg headers (optional SVG library)
if (TARGET Resvg::Resvg)
  #include_directories(${Resvg_INCLUDE_DIRS})
  target_link_libraries(openshot PUBLIC Resvg::Resvg)

  target_compile_definitions(openshot PUBLIC -DUSE_RESVG=1)
  list(APPEND CMAKE_SWIG_FLAGS -DUSE_RESVG=1)

  set(HAVE_RESVG TRUE CACHE BOOL "Building with Resvg support" FORCE)
  mark_as_advanced(HAVE_RESVG)
endif()

################# BLACKMAGIC DECKLINK ###################
# Find BlackMagic DeckLinkAPI libraries
if (ENABLE_BLACKMAGIC)
  find_package(BlackMagic)

  if (BLACKMAGIC_FOUND)
    # BlackMagic related files
    target_sources(openshot PRIVATE
      DecklinkInput.cpp
      DecklinkReader.cpp
      DecklinkOutput.cpp
      DecklinkWriter.cpp)

    # Include Blackmagic headers (needed for compile)
    target_include_directories(openshot PRIVATE ${BLACKMAGIC_INCLUDE_DIR})

    # Link libopenshot with BlackMagic libs
    target_link_libraries(openshot PUBLIC ${BLACKMAGIC_LIBRARY_DIR})

    # define a preprocessor macro (used in the C++)
    target_compile_definitions(openshot PUBLIC -DUSE_BLACKMAGIC=1)
    list(APPEND CMAKE_SWIG_FLAGS -DUSE_BLACKMAGIC=1)
  endif()
endif()

###############  LINK LIBRARY  #################
# Link remaining dependency libraries
if(DEFINED PROFILER)
  target_link_libraries(openshot PUBLIC ${PROFILER})
endif()

if(WIN32)
  # Required for exception handling on Windows
  target_link_libraries(openshot PUBLIC "imagehlp" "dbghelp" )
endif()

###
### INSTALL HEADERS & LIBRARY
###

# Install primary library
install(TARGETS openshot
  ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR}
  LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
  RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
  INCLUDES DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}/libopenshot)

install(
  DIRECTORY .
  DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}/libopenshot
  FILES_MATCHING PATTERN "*.h"
)

############### CPACK PACKAGING ##############
if(MINGW)
  set(CPACK_GENERATOR "NSIS")
endif()
if(UNIX AND NOT APPLE)
  set(CPACK_GENERATOR "DEB")
endif()
#if(UNIX AND APPLE)
#  set(CPACK_GENERATOR "DragNDrop")
#endif()
set(CPACK_DEBIAN_PACKAGE_MAINTAINER "Jonathan Thomas") #required

include(CPack)
