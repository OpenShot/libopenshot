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

set(TEST_MEDIA_PATH "${PROJECT_SOURCE_DIR}/src/examples/")

################ WINDOWS ##################
# Set some compiler options for Windows
# required for libopenshot-audio headers
if (WIN32)
	STRING(REPLACE "/" "\\\\" TEST_MEDIA_PATH TEST_MEDIA_PATH)
  #add_definitions( -DIGNORE_JUCE_HYPOT=1 )
	SET(CMAKE_CXX_FLAGS " ${CMAKE_CXX_FLAGS} -include cmath")
endif()

add_definitions( -DTEST_MEDIA_PATH="${TEST_MEDIA_PATH}" )

################### UNITTEST++ #####################
# Find UnitTest++ libraries (used for unit testing)
FIND_PACKAGE(UnitTest++ REQUIRED)

# Include UnitTest++ headers (needed for compile)
include_directories(${UNITTEST++_INCLUDE_DIR})

################ IMAGE MAGICK ##################
# Find the ImageMagick++ library
find_package(ImageMagick COMPONENTS Magick++ MagickWand MagickCore)
if(ImageMagick_FOUND)
	# Include ImageMagick++ headers (needed for compile)
	include_directories(${ImageMagick_INCLUDE_DIRS})

	# define a global var (used in the C++)
	add_definitions( -DUSE_IMAGEMAGICK=1 )

  #Set the Quantum Depth that ImageMagick was built with (default to 16 bits)
  if (MAGICKCORE_QUANTUM_DEPTH)
  	add_definitions( -DMAGICKCORE_QUANTUM_DEPTH=${MAGICKCORE_QUANTUM_DEPTH} )
  else()
  	add_definitions( -DMAGICKCORE_QUANTUM_DEPTH=16 )
  endif()

  if (MAGICKCORE_HDRI_ENABLE)
  	add_definitions( -DMAGICKCORE_HDRI_ENABLE=${MAGICKCORE_HDRI_ENABLE} )
  else()
  	add_definitions( -DMAGICKCORE_HDRI_ENABLE=0 )
  endif()

endif()

################# LIBOPENSHOT-AUDIO ###################
# Find JUCE-based openshot Audio libraries
find_package(OpenShotAudio 0.1.8 REQUIRED)

# Include Juce headers (needed for compile)
include_directories(${LIBOPENSHOT_AUDIO_INCLUDE_DIRS})


################# BLACKMAGIC DECKLINK ###################
if (ENABLE_BLACKMAGIC)
	# Find BlackMagic DeckLinkAPI libraries
	find_package(BlackMagic)

	if (BLACKMAGIC_FOUND)
		# Include Blackmagic headers (needed for compile)
		include_directories(${BLACKMAGIC_INCLUDE_DIR})
	endif()
endif()


################### RESVG #####################
# Find resvg library (used for rendering svg files)
find_package(RESVG)

# Include resvg headers (optional SVG library)
if (RESVG_FOUND)
	include_directories(${RESVG_INCLUDE_DIRS})
endif()


###############  SET TEST SOURCE FILES  #################
# Each testname is expected to correspond to a file ${testname}_Tests.cpp
# in the tests/ directory
set(OPENSHOT_TESTS
	Cache
	Clip
	Color
	Coordinate
	ReaderBase
	ImageWriter
	FFmpegReader
	FFmpegWriter
	Fraction
	FrameMapper
	KeyFrame
	Point
	Settings
	Timeline )

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
