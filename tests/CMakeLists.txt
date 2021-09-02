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

# Allow spaces in test names
if(POLICY CMP0110)
  cmake_policy(SET CMP0110 NEW)
endif()

# Test media path, used by unit tests for input data
file(TO_NATIVE_PATH "${PROJECT_SOURCE_DIR}/examples/" TEST_MEDIA_PATH)

################# BLACKMAGIC DECKLINK ###################
if(ENABLE_BLACKMAGIC)
	# Find BlackMagic DeckLinkAPI libraries
	find_package(BlackMagic)

	if(BLACKMAGIC_FOUND)
		# Include Blackmagic headers (needed for compile)
		include_directories(${BLACKMAGIC_INCLUDE_DIR})
	endif()
endif()

###
###  TEST SOURCE FILES
###
set(OPENSHOT_TESTS
  CacheDisk
  CacheMemory
  Clip
  Color
  Coordinate
  DummyReader
  FFmpegReader
  FFmpegWriter
  Fraction
  Frame
  FrameMapper
  KeyFrame
  Point
  QtImageReader
  ReaderBase
  Settings
  Timeline
)

# ImageMagick related test files
if($CACHE{HAVE_IMAGEMAGICK})
  list(APPEND OPENSHOT_TESTS
    ImageWriter
  )
endif()

# OPENCV RELATED TEST FILES
if($CACHE{HAVE_OPENCV})
  list(APPEND OPENSHOT_TESTS
    CVTracker
    CVStabilizer
    # CVObjectDetection
  )
endif()

###
### Catch2 unit tests
###
include(CTest)
include(Catch)

if(NOT BUILD_TESTING)
  return()
endif()

# Create object library for test executable main(),
# to avoid recompiling for every test
add_library(catch-main OBJECT catch_main.cpp)
target_link_libraries(catch-main PUBLIC Catch2::Catch2)

foreach(tname ${OPENSHOT_TESTS})
  add_executable(openshot-${tname}-test ${tname}.cpp $<TARGET_OBJECTS:catch-main>)
  target_compile_definitions(openshot-${tname}-test PRIVATE
    TEST_MEDIA_PATH="${TEST_MEDIA_PATH}"
  )
  target_link_libraries(openshot-${tname}-test Catch2::Catch2 openshot)
  # Automatically configure CTest targets from Catch2 test cases
  catch_discover_tests(
    openshot-${tname}-test
    TEST_PREFIX ${tname}:
    PROPERTIES
      LABELS ${tname}
  )
  list(APPEND CATCH2_TEST_TARGETS openshot-${tname}-test)
  list(APPEND CATCH2_TEST_NAMES ${tname})
endforeach()
# Add an additional special-case test, for an envvar-dependent setting
add_test(NAME [=["Settings:Debug logging (enabled)"]=]
  COMMAND
    openshot-Settings-test "[environment]"
  WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}
)
set_tests_properties([=["Settings:Debug logging (enabled)"]=]
  PROPERTIES
    LABELS Settings
    ENVIRONMENT "LIBOPENSHOT_DEBUG=1"
)

# Export target list for coverage use
set(UNIT_TEST_TARGETS ${CATCH2_TEST_TARGETS} PARENT_SCOPE)
set(UNIT_TEST_NAMES ${CATCH2_TEST_NAMES} PARENT_SCOPE)
