####################### CMakeLists.txt (libopenshot) #########################
# @brief CMake build file for libopenshot (used to generate makefiles)
# @author Jonathan Thomas <jonathan@openshot.org>
# @author FeRD (Frank Dana) <ferdnyc@gmail.com>
#
# @section LICENSE
#
# Copyright (c) 2008-2020 OpenShot Studios, LLC
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

add_executable(openshot-html-test ExampleHtml.cpp)
target_link_libraries(openshot-html-test openshot Qt5::Gui)

############### PLAYER EXECUTABLE ################
# Create test executable
add_executable(openshot-player qt-demo/main.cpp)

set_target_properties(openshot-player PROPERTIES AUTOMOC ON)

# Link test executable to the new library
target_link_libraries(openshot-player openshot)

############### TEST BLACKMAGIC CAPTURE APP ################
if (BLACKMAGIC_FOUND)
	# Create test executable
	add_executable(openshot-blackmagic
			examples/ExampleBlackmagic.cpp)

	# Link test executable to the new library
	target_link_libraries(openshot-blackmagic openshot)
endif()
