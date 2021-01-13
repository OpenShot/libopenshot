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

# Dependencies
find_package(Protobuf 3)
include_directories(${PROTOBUF_INCLUDE_DIRS})

file(GLOB ProtoFiles "${CMAKE_CURRENT_SOURCE_DIR}/*.proto")
PROTOBUF_GENERATE_CPP(ProtoSources ProtoHeaders ${ProtoFiles})
add_library(openshot_protobuf SHARED ${ProtoSources} ${ProtoHeaders})

set(ProtobufMessagePath ${CMAKE_CURRENT_BINARY_DIR}
    CACHE INTERNAL "Path to generated protobuf header files.")

target_link_libraries(openshot_protobuf ${Boost_LIBRARIES} ${PROTOBUF_LIBRARY})

# Set SONAME and other library properties
set_target_properties(openshot_protobuf PROPERTIES
        VERSION ${PROJECT_VERSION}
        SOVERSION ${PROJECT_SO_VERSION}
        INSTALL_NAME_DIR "${CMAKE_INSTALL_PREFIX}/lib"
        )

# Install primary library
install(TARGETS openshot_protobuf
        LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR})
        
