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

# Dependencies
find_package(Qt5 COMPONENTS Gui REQUIRED)

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
target_link_libraries(openshot-html-example openshot Qt5::Gui)

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
