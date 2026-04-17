####################### CMakeLists.txt (libopenshot) #########################
# @brief CMake build file for libopenshot (used to generate makefiles)
# @author Jonathan Thomas <jonathan@openshot.org>
# @author FeRD (Frank Dana) <ferdnyc@gmail.com>
#
# @section LICENSE
#
# Copyright (c) 2008-2020 OpenShot Studios, LLC
#
# SPDX-License-Identifier: LGPL-3.0-or-later

include(GNUInstallDirs)

# Qt selection matches the main library (USE_QT6 option, CMake 3.16+ needed for Qt6)
set(_qt6_allowed TRUE)
if(CMAKE_VERSION VERSION_LESS "3.16")
  set(_qt6_allowed FALSE)
  if(USE_QT6 STREQUAL "ON")
    message(FATAL_ERROR "USE_QT6=ON requires CMake 3.16 or newer")
  endif()
endif()
if(USE_QT6 STREQUAL "OFF")
  set(_qt6_allowed FALSE)
endif()

set(_qt_selection "")
if(USE_QT6 STREQUAL "ON")
  if(NOT _qt6_allowed)
    message(FATAL_ERROR "Qt6 support disabled by CMake version; set USE_QT6=OFF or upgrade CMake")
  endif()
  set(_qt_selection "Qt6")
elseif(USE_QT6 STREQUAL "OFF")
  set(_qt_selection "Qt5")
else()
  if(_qt6_allowed)
    find_package(Qt6 COMPONENTS Core QUIET)
    if(Qt6_FOUND)
      set(_qt_selection "Qt6")
    endif()
  endif()
  if(NOT _qt_selection)
    set(_qt_selection "Qt5")
  endif()
endif()

if(_qt_selection STREQUAL "Qt6")
  set(QT_VERSION_MAJOR 6)
else()
  set(QT_VERSION_MAJOR 5)
endif()

find_package(Qt${QT_VERSION_MAJOR} COMPONENTS Gui REQUIRED)

############### CLI EXECUTABLES ################
# Create test executable
add_executable(openshot-example Example.cpp)

# Define path to test input files
file(TO_NATIVE_PATH "${PROJECT_SOURCE_DIR}/examples/" TEST_MEDIA_PATH)
target_compile_definitions(openshot-example PRIVATE
	-DTEST_MEDIA_PATH="${TEST_MEDIA_PATH}" )

# Link test executable to the new library
target_link_libraries(openshot-example openshot)

add_executable(openshot-html-example ExampleHtml.cpp)
target_link_libraries(openshot-html-example openshot Qt${QT_VERSION_MAJOR}::Gui)

if(ENABLE_VULKAN_BENCHMARK)
  find_package(Vulkan QUIET)
  find_package(FFmpeg QUIET COMPONENTS avfilter)
  find_program(GLSLANG_VALIDATOR glslangValidator)
  find_package(PkgConfig QUIET)
  if(PKG_CONFIG_FOUND)
    pkg_check_modules(VULKAN_PC QUIET vulkan)
  endif()

  if(Vulkan_FOUND AND TARGET FFmpeg::avfilter AND GLSLANG_VALIDATOR)
    set(VULKAN_BENCHMARK_SHADER_DIR "${CMAKE_CURRENT_BINARY_DIR}/vulkan")
    file(MAKE_DIRECTORY "${VULKAN_BENCHMARK_SHADER_DIR}")
    set(VULKAN_DIRECT_PREVIEW_SHADER "${VULKAN_BENCHMARK_SHADER_DIR}/direct_preview.comp.spv")
    add_custom_command(
      OUTPUT "${VULKAN_DIRECT_PREVIEW_SHADER}"
      COMMAND "${GLSLANG_VALIDATOR}" -V -S comp
              -o "${VULKAN_DIRECT_PREVIEW_SHADER}"
              "${CMAKE_CURRENT_SOURCE_DIR}/vulkan/direct_preview.comp"
      DEPENDS "${CMAKE_CURRENT_SOURCE_DIR}/vulkan/direct_preview.comp"
      VERBATIM
    )
    add_custom_target(openshot-vulkan-benchmark-shaders DEPENDS "${VULKAN_DIRECT_PREVIEW_SHADER}")

    add_executable(openshot-vulkan-benchmark VulkanBenchmark.cpp)
    add_dependencies(openshot-vulkan-benchmark openshot-vulkan-benchmark-shaders)
    target_compile_definitions(openshot-vulkan-benchmark PRIVATE
      -DTEST_MEDIA_PATH="${TEST_MEDIA_PATH}"
      -DUSE_VULKAN_BENCHMARK=1
      -DVULKAN_DIRECT_PREVIEW_SHADER_PATH="${VULKAN_DIRECT_PREVIEW_SHADER}"
    )
    if(VULKAN_PC_FOUND)
      # Prefer Vulkan headers from pkg-config when available (e.g. local headers
      # used by custom FFmpeg builds), to avoid header/API mismatches.
      target_include_directories(openshot-vulkan-benchmark PRIVATE ${VULKAN_PC_INCLUDE_DIRS})
      target_compile_options(openshot-vulkan-benchmark PRIVATE ${VULKAN_PC_CFLAGS_OTHER})
    endif()
    target_link_libraries(openshot-vulkan-benchmark
      openshot
      Qt5::Gui
      Vulkan::Vulkan
      FFmpeg::avfilter
    )
  else()
    message(WARNING "ENABLE_VULKAN_BENCHMARK was requested, but Vulkan, FFmpeg avfilter, or glslangValidator was not found. Skipping openshot-vulkan-benchmark.")
  endif()
endif()

############### PLAYER EXECUTABLE ################
# Create test executable
add_executable(openshot-player qt-demo/main.cpp)

set_target_properties(openshot-player
  PROPERTIES
    AUTOMOC ON
    WIN32_EXECUTABLE ON
)

# Link test executable to the new library
target_link_libraries(openshot-player openshot)

############### OPENCV EXAMPLE ################
#if (DEFINED CACHE{HAVE_OPENCV})
#	# Create test executable
#	add_executable(openshot-example-opencv
#			Example_opencv.cpp)
#
#	target_compile_definitions(openshot-example-opencv PRIVATE
#	                           -DTEST_MEDIA_PATH="${TEST_MEDIA_PATH}" )
#	# Link test executable to the new library
#	target_link_libraries(openshot-example-opencv openshot)
#endif()
