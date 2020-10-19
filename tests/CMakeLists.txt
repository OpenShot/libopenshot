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

include(CTest)

if(ENABLE_COVERAGE)
  include(CodeCoverage)
endif()

# Test media path, used by unit tests for input data
file(TO_NATIVE_PATH "${PROJECT_SOURCE_DIR}/examples/" TEST_MEDIA_PATH)
add_definitions( -DTEST_MEDIA_PATH="${TEST_MEDIA_PATH}" )

################### UNITTEST++ #####################
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


################# BLACKMAGIC DECKLINK ###################
if(ENABLE_BLACKMAGIC)
	# Find BlackMagic DeckLinkAPI libraries
	find_package(BlackMagic)

	if(BLACKMAGIC_FOUND)
		# Include Blackmagic headers (needed for compile)
		include_directories(${BLACKMAGIC_INCLUDE_DIR})
	endif()
endif()


###############  SET TEST SOURCE FILES  #################
# Each testname is expected to correspond to a file ${testname}_Tests.cpp
# in the tests/ directory
set(OPENSHOT_TESTS
	Cache
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
	Settings
	Timeline
)

foreach(testname IN LISTS OPENSHOT_TESTS)
  # Create unit test executable (openshot-${testname}-test)
  add_executable(test-${testname}
    tests.cpp
    "${testname}_Tests.cpp")

  # Link libraries to the new executable
  target_link_libraries(test-${testname}
    openshot
    ${UNITTEST++_LIBRARY})

  add_test(NAME ${testname} COMMAND test-${testname})
  list(APPEND TEST_TARGETS test-${testname})

  if(ENABLE_COVERAGE)
    include(CodeCoverage)
    setup_target_for_coverage_lcov(
      NAME ${testname}-coverage
      LCOV_ARGS "--no-external"
      EXECUTABLE test-${testname}
      DEPENDENCIES openshot test-${testname})
    list(APPEND COVERAGE_TARGETS ${testname}-coverage)
  endif()
endforeach()

set(TEST_TARGETS ${TEST_TARGETS} PARENT_SCOPE)
set(COVERAGE_TARGETS ${COVERAGE_TARGETS} PARENT_SCOPE)

##### RUNNING TESTS (make os_test / make test) #####
# Hook up the 'make os_test' target to the test executables
add_custom_target(os_test
  COMMAND ${CMAKE_MAKE_PROGRAM} test
  COMMENT "Building and running unit tests")
add_dependencies(os_test ${TEST_TARGETS})
