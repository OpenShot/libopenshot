####################### CMakeLists.txt (libopenshot) #########################
# @brief CMake build file for libopenshot (used to generate makefiles)
# @author Jonathan Thomas <jonathan@openshot.org>
#
# @section LICENSE
#
# Copyright (c) 2008-2019 OpenShot Studios, LLC
#
# SPDX-License-Identifier: LGPL-3.0-or-later

# Collect and display summary of options/dependencies
include(FeatureSummary)

include(GNUInstallDirs)

# Enable IN_LIST in older CMake
if (POLICY CMP0057)
  cmake_policy(SET CMP0057 NEW)
endif()

###############  PROFILING  #################
#set (CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -fno-omit-frame-pointer -fsanitize=address")
#set (CMAKE_LINKER_FLAGS "${CMAKE_LINKER_FLAGS} -fno-omit-frame-pointer -fsanitize=address")
#set(PROFILER "/usr/lib/x86_64-linux-gnu/libprofiler.so.0")
#set(PROFILER "/usr/lib/x86_64-linux-gnu/libtcmalloc.so.4")

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
  AudioDevices.cpp
  AudioReaderSource.cpp
  AudioResampler.cpp
  AudioWaveformer.cpp
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
  PlayerBase.cpp
  Point.cpp
  Profiles.cpp
  QtHtmlReader.cpp
  QtImageReader.cpp
  QtPlayer.cpp
  QtTextReader.cpp
  Settings.cpp
  TimelineBase.cpp
  Timeline.cpp
  TrackedObjectBase.cpp
  ZmqLogger.cpp
  )

# OpenCV related classes
set(OPENSHOT_CV_SOURCES
  CVTracker.cpp
  CVStabilization.cpp
  ClipProcessingJobs.cpp
  CVObjectDetection.cpp
  TrackedObjectBBox.cpp
  effects/Stabilizer.cpp
  effects/Tracker.cpp
  effects/ObjectDetection.cpp
  effects/Outline.cpp
  ./sort_filter/sort.cpp
  ./sort_filter/Hungarian.cpp
  ./sort_filter/KalmanTracker.cpp)

# Video effects
set(EFFECTS_SOURCES
  effects/Bars.cpp
  effects/Blur.cpp
  effects/Brightness.cpp
  effects/Caption.cpp
  effects/ChromaKey.cpp
  effects/ColorMap.cpp
  effects/ColorShift.cpp
  effects/Crop.cpp
  effects/Deinterlace.cpp
  effects/Hue.cpp
  effects/LensFlare.cpp
  effects/Mask.cpp
  effects/Negate.cpp
  effects/Pixelate.cpp
  effects/Saturation.cpp
  effects/Sharpen.cpp
  effects/Shift.cpp
  effects/SphericalProjection.cpp
  effects/Wave.cpp
  audio_effects/STFT.cpp
  audio_effects/Noise.cpp
  audio_effects/Delay.cpp
  audio_effects/Echo.cpp
  audio_effects/Distortion.cpp
  audio_effects/ParametricEQ.cpp
  audio_effects/Compressor.cpp
  audio_effects/Expander.cpp
  audio_effects/Robotization.cpp
  audio_effects/Whisperization.cpp)

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
  find_package(OpenShotAudio 0.4.0 REQUIRED)
endif()
target_link_libraries(openshot PUBLIC OpenShot::Audio)

###
### ImageMagick
###

# Find the ImageMagick++ library
if (ENABLE_MAGICK)
  find_package(ImageMagick COMPONENTS Magick++ MagickCore)

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
      MagickUtilities.cpp
      ImageReader.cpp
      ImageWriter.cpp
      TextReader.cpp
    )

    # define a preprocessor macro (used in the C++ source)
    target_compile_definitions(openshot PUBLIC USE_IMAGEMAGICK=1)

    # Link with ImageMagick library
    target_link_libraries(openshot PUBLIC ImageMagick::Magick++)

    set(HAVE_IMAGEMAGICK TRUE CACHE BOOL "Building with ImageMagick support" FORCE)
    mark_as_advanced(HAVE_IMAGEMAGICK)
  endif()
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
    add_library(jsoncpp_lib OBJECT)
    set_target_properties(jsoncpp_lib PROPERTIES
      POSITION_INDEPENDENT_CODE TRUE)
    target_include_directories(jsoncpp_lib PUBLIC
      "${PROJECT_SOURCE_DIR}/thirdparty/jsoncpp")
    target_sources(jsoncpp_lib PRIVATE "${PROJECT_SOURCE_DIR}/thirdparty/jsoncpp/jsoncpp.cpp")
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

  target_compile_definitions(openshot PUBLIC USE_RESVG=1)

  set(HAVE_RESVG TRUE CACHE BOOL "Building with Resvg support" FORCE)
  mark_as_advanced(HAVE_RESVG)
else()
  set(HAVE_RESVG FALSE CACHE BOOL "Building with Resvg support" FORCE)
endif()

###
### Qt Toolkit
###

set(_qt_components Core Gui Widgets)

# We also need QtSvg unless we have Resvg insetead.
if(NOT HAVE_RESVG)
  list(APPEND _qt_components Svg)
endif()

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
find_package(FFmpeg REQUIRED
  COMPONENTS avcodec avformat avutil swscale
  OPTIONAL_COMPONENTS swresample avresample
)

set(all_comps avcodec avformat avutil swscale)
set(version_comps avcodec avformat avutil)

# Pick a resampler.  Prefer swresample if possible
if(TARGET FFmpeg::swresample)
  set(resample_lib swresample)
  set(USE_SW TRUE)
else()
  set(resample_lib avresample)
  set(USE_SW FALSE)
endif()
list(APPEND all_comps ${resample_lib})

foreach(ff_comp IN LISTS all_comps)
  if(TARGET FFmpeg::${ff_comp})
    target_link_libraries(openshot PUBLIC FFmpeg::${ff_comp})
    # Keep track of some FFmpeg lib versions, to embed in our version header
    if(${ff_comp} IN_LIST version_comps AND FFmpeg_${ff_comp}_VERSION)
      string(TOUPPER ${ff_comp} v_name)
      set(${v_name}_VERSION_STR ${FFmpeg_${ff_comp}_VERSION} CACHE STRING "${ff_comp} version used" FORCE)
      mark_as_advanced(${v_name}_VERSION_STR)
    endif()
  endif()
endforeach()

# Indicate which resampler we linked with, and set a config header flag
add_feature_info("FFmpeg ${resample_lib}" TRUE "Audio resampling uses ${resample_lib}")
# Set the appropriate flag in OpenShotVersion.h
set(FFMPEG_USE_SWRESAMPLE ${USE_SW} CACHE BOOL "libswresample used for audio resampling" FORCE)
mark_as_advanced(FFMPEG_USE_SWRESAMPLE)

# Version check for hardware-acceleration code
if(USE_HW_ACCEL AND FFmpeg_avcodec_VERSION)
  if(${FFmpeg_avcodec_VERSION} VERSION_GREATER "57.106")
    set(HAVE_HW_ACCEL TRUE)
  endif()
endif()

# Hwaccel preprocessor define (for source #ifdefs)
if (NOT USE_HW_ACCEL)
  target_compile_definitions(openshot PUBLIC USE_HW_ACCEL=0)
elseif (HAVE_HW_ACCEL)
  target_compile_definitions(openshot PUBLIC USE_HW_ACCEL=1)
endif()

# Include in feature summary
if(USE_HW_ACCEL AND HAVE_HW_ACCEL)
  set(FFMPEG_HARDWARE_ACCELERATION TRUE)
elseif(USE_HW_ACCEL)
  set(FFMPEG_HARDWARE_ACCELERATION "NOTFOUND")
else()
  set(FFMPEG_HARDWARE_ACCELERATION FALSE)
endif()

# Report HWAccel status, unless it's indeterminate
if (NOT FFMPEG_HARDWARE_ACCELERATION STREQUAL "NOTFOUND")
  set(_hwaccel_help "GPU-accelerated routines (FFmpeg 3.4+)")
  add_feature_info("FFmpeg hwaccel" FFMPEG_HARDWARE_ACCELERATION ${_hwaccel_help})
endif()

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
### Babl
###

# Find babl library for colourspace conversion (used for advanced chroma keying
find_package(babl)

if (babl_FOUND)
  set(HAVE_BABL TRUE CACHE BOOL "Building with babl support" FORCE)
  mark_as_advanced(HAVE_BABL)
endif()

if (TARGET babl_lib)
  target_compile_definitions(openshot PUBLIC USE_BABL=1)
  target_link_libraries(openshot PUBLIC babl_lib)
endif ()

################## OPENCV ###################
if(ENABLE_OPENCV)
  find_package(OpenCV 4)
  if(NOT OpenCV_FOUND)
    set(ENABLE_OPENCV FALSE CACHE BOOL
      "Build with OpenCV algorithms (requires Protobuf 3)" FORCE)
  # If we have version 4.5.1, all hope is lost
  elseif(OpenCV_VERSION VERSION_EQUAL "4.5.1")
    message(WARNING [[Incompatible OpenCV version detected
OpenCV version 4.5.1 contains header errors which make it unable to be used with OpenShot. OpenCV support wil be disabled. Upgrade to OpenCV 4.5.2+ or downgrade to 4.5.0 or earlier, to enable OpenCV.
See https://github.com/opencv/opencv/issues/19260]])
    set(ENABLE_OPENCV FALSE CACHE BOOL
      "Build with OpenCV algorithms (requires Protobuf 3)" FORCE)
  else()
    ###
    ### Protocol Buffers
    ###
    find_package(Protobuf 3 REQUIRED)

    # Create a target for libprotobuf, if necessary (CMake < 3.9)
    if(NOT TARGET protobuf::libprotobuf)
      add_library(protobuf_TARGET INTERFACE)
      target_include_directories(protobuf_TARGET
        INTERFACE ${Protobuf_INCLUDE_DIRS})
      target_link_libraries(protobuf_TARGET INTERFACE ${Protobuf_LIBRARIES})
      add_library(protobuf::libprotobuf ALIAS protobuf_TARGET)
    endif()

    # Create libopenshot_protobuf
    set(PROTOBUF_SOURCES
      "objdetectdata.proto"
      "stabilizedata.proto"
      "trackerdata.proto"
    )
    PROTOBUF_GENERATE_CPP(ProtoSources ProtoHeaders ${PROTOBUF_SOURCES})

    target_sources(openshot PRIVATE ${ProtoSources} ${ProtoHeaders})
    target_link_libraries(openshot PRIVATE protobuf::libprotobuf)

    ###
    ### OpenCV
    ###

    # Add OpenCV source files
    target_sources(openshot PRIVATE
      ${OPENSHOT_CV_SOURCES}
    )
    target_compile_definitions(openshot PUBLIC USE_OPENCV=1)
    target_link_libraries(openshot PUBLIC
      opencv_core
      opencv_video
      opencv_highgui
      opencv_dnn
      opencv_tracking
    )
    set(HAVE_OPENCV TRUE CACHE BOOL "Building with OpenCV effects" FORCE)
    mark_as_advanced(HAVE_OPENCV)

    # Keep track of OpenCV version, to embed in our version header
    set(OPENCV_VERSION_STR "${OpenCV_VERSION}" CACHE STRING "OpenCV version linked with" FORCE)
    mark_as_advanced(OPENCV_VERSION_STR)

    # We may need to use Tracker as opencv::legacy::Tracker
    if(OpenCV_VERSION VERSION_GREATER "4.5.1")
      target_compile_definitions(openshot PUBLIC USE_LEGACY_TRACKER=1)
    endif()

  endif()
endif()  # ENABLE_OPENCV
add_feature_info("OpenCV algorithms" ENABLE_OPENCV "Use OpenCV algorithms")

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

# Install libraries
install(TARGETS openshot
  COMPONENT runtime
  ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR}
  LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
  RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
  INCLUDES DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}/libopenshot)

install(DIRECTORY .
  COMPONENT devel
  DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}/libopenshot
  FILES_MATCHING PATTERN "*.h"
)


# On Windows, we copy project output DLLs into the tests dir
# so that the unit test executables can find them
if(CMAKE_VERSION VERSION_GREATER 3.13 AND WIN32 AND BUILD_TESTING)
  # Copy the DLLs immediately after they're built
  add_custom_command(TARGET openshot POST_BUILD
    COMMAND
      ${CMAKE_COMMAND} -E copy_if_different
      "$<TARGET_FILE:openshot>"
      "${PROJECT_BINARY_DIR}/tests/"
    BYPRODUCTS
      "${PROJECT_BINARY_DIR}/tests/libopenshot.dll"
    COMMENT
      "Copying libopenshot DLL to unit test directory"
  )

  # Also copy libopenshot-audio DLL
  get_target_property(AUDIO_LIB_PATH OpenShot::Audio LOCATION)
  add_custom_target(test-install-audio-dll ALL
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
      "${AUDIO_LIB_PATH}"
      "${PROJECT_BINARY_DIR}/tests/"
    BYPRODUCTS
      "${PROJECT_BINARY_DIR}/tests/libopenshot-audio.dll"
    COMMENT
      "Copying OpenShotAudio library DLL to unit test directory"
  )
  add_dependencies(test-install-audio-dll OpenShot::Audio)
endif()


############### CPACK PACKAGING ##############
if(MINGW)
  set(CPACK_GENERATOR "NSIS")
endif()
if(UNIX AND NOT APPLE)
  set(CPACK_GENERATOR "DEB")
endif()
set(CPACK_DEBIAN_PACKAGE_MAINTAINER "Jonathan Thomas") #required

include(CPack)
