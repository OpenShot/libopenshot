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
  ReaderBase
  ImageWriter
  FFmpegReader
  FFmpegWriter
  Fraction
  Frame
  FrameMapper
  KeyFrame
  Point
  QtImageReader
  Settings
  Timeline)

###
### OPENCV RELATED TEST FILES
###
if(ENABLE_OPENCV)
  list(APPEND OPENSHOT_TESTS
    CVTracker
    CVStabilizer
    # CVObjectDetection
  )
endif()

###
### Catch2 (new) unit tests
###
if (TESTS_ENABLED AND USE_CATCH2)
  message (STATUS "Tests enabled, test executables will be compiled")
  include(Catch)

  include(CTest)

  # Create object library for test executable main(),
  # to avoid recompiling for every test
  add_library(catch-main OBJECT catch_main.cpp)

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
    )
    list(APPEND CATCH2_TEST_TARGETS openshot-${tname}-test)
	endforeach()
  # Export target list for coverage use
  set(CATCH2_TEST_TARGETS ${CATCH2_TEST_TARGETS} PARENT_SCOPE)
endif()

###
### UNITTEST++ (old) unit tests
###
if (TESTS_ENABLED AND NOT USE_CATCH2)
  # Find UnitTest++ libraries (used for unit testing)
  find_package(UnitTest++)

  if (NOT UnitTest++_FOUND)
    set(TESTS_ENABLED OFF PARENT_SCOPE)
    return()
  endif()

  # Include UnitTest++ headers (needed for compile)
  include_directories(${UnitTest++_INCLUDE_DIRS})

  set_package_properties(UnitTest++ PROPERTIES
    TYPE RECOMMENDED
    PURPOSE "Unit testing framework")

  ################ TESTER EXECUTABLE #################
  # Create unit test executable (openshot-test)
  message (STATUS "Tests enabled, test executable will be built as tests/openshot-test")

  foreach(tname ${OPENSHOT_TESTS})
    list(APPEND TEST_SOURCES cppunittest/${tname}_Tests.cpp)
  endforeach()

  add_executable(openshot-test
    cppunittest/tests.cpp
    ${TEST_SOURCES}
  )
  target_compile_definitions(openshot-test PRIVATE
    TEST_MEDIA_PATH=${TEST_MEDIA_PATH}
  )
  # Link libraries to the new executable
  target_link_libraries(openshot-test
    openshot
    ${UnitTest++_LIBRARIES}
  )
  set(UNIT_TEST_TARGETS openshot-test PARENT_SCOPE)
endif()
