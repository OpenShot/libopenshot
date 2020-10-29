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

####################### OPENCV ##########################
find_package( OpenCV 4 )
if (OpenCV_FOUND)
  add_definitions( -DUSE_OPENCV=1 )
else()
  message("\nOPENCV NOT FOUND, OPENCV RELATED TESTS ARE DISABLED\n")
endif()

################ PROTOBUF ##################
if (OpenCV_FOUND)
  find_package(Protobuf 3)
  
  if (NOT Protobuf_FOUND)
    # Protobuf is required when compiling with opencv
    message(FATAL_ERROR "\nPLEASE INSTALL PROTOBUF. Protobuf is required when compiling with opencv.\n")
  endif()
  include_directories(${PROTOBUF_INCLUDE_DIR})
endif()

###############  SET TEST SOURCE FILES  #################
set(OPENSHOT_TEST_FILES
  Cache_Tests.cpp
  Clip_Tests.cpp
  Color_Tests.cpp
  Coordinate_Tests.cpp
  DummyReader_Tests.cpp
  ReaderBase_Tests.cpp
  ImageWriter_Tests.cpp
  FFmpegReader_Tests.cpp
  FFmpegWriter_Tests.cpp
  Fraction_Tests.cpp
  Frame_Tests.cpp
  FrameMapper_Tests.cpp
  KeyFrame_Tests.cpp
  Point_Tests.cpp
  Settings_Tests.cpp
  Timeline_Tests.cpp)

##########  SET OPENCV RELATED TEST FILES  ###############
set(OPENSHOT_CV_TEST_FILES
  CVTracker_Tests.cpp
  CVStabilizer_Tests.cpp
  # CVObjectDetection_Tests.cpp
  )

################ TESTER EXECUTABLE #################
# Create unit test executable (openshot-test)
message (STATUS "Tests enabled, test executable will be built as tests/openshot-test")
if (OpenCV_FOUND)
  add_executable(openshot-test
    tests.cpp
    ${OPENSHOT_TEST_FILES} 
    ${OPENSHOT_CV_TEST_FILES})
  
  # Link libraries to the new executable
  target_link_libraries(openshot-test openshot ${UnitTest++_LIBRARIES} ${OpenCV_LIBS} ${PROTOBUF_LIBRARY})

else()
  add_executable(openshot-test
    tests.cpp
    ${OPENSHOT_TEST_FILES} )

  # Link libraries to the new executable
  target_link_libraries(openshot-test openshot ${UnitTest++_LIBRARIES})
endif()

##### RUNNING TESTS (make os_test / make test) #####
# Hook up the 'make os_test' target to the 'openshot-test' executable
add_custom_target(os_test COMMAND openshot-test)

# Also hook up 'make test', if possible
# This requires CMake 3.11+, where the CMP0037 policy
# configured to 'NEW' mode will not reserve target names
# unless the corresponding feature is actually used
if (POLICY CMP0037)
	cmake_policy(SET CMP0037 NEW)
endif()
if (CMAKE_VERSION VERSION_GREATER 3.11)
	message(STATUS "Cmake 3.11+ detected, enabling 'test' target")
	add_custom_target(test COMMAND openshot-test)
	set(TEST_TARGET_NAME "test")
else()
  set(TEST_TARGET_NAME "os_test")
endif()
add_feature_info("Testrunner" ENABLE_TESTS "Run unit tests with 'make ${TEST_TARGET_NAME}'")
