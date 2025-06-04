####################### CMakeLists.txt (libopenshot) #########################
# @brief CMake build file for libopenshot-godot (used to generate GDExtension)
# @author Jonathan Thomas <jonathan@openshot.org>
#
# @section LICENSE
#
# Copyright (c) 2008-2025 OpenShot Studios, LLC
#
# SPDX-License-Identifier: LGPL-3.0-or-later

cmake_minimum_required(VERSION 3.17)

project(libopenshot_godot)

# Clone and build GoDot CPP
# git submodule add https://github.com/godotengine/godot-cpp.git external/godot-cpp
# git submodule update --init --recursive
# cd external/godot-cpp
# scons -c
# scons platform=linux generate_bindings=yes -j$(nproc)
set(GODOT_CPP_PATH ${CMAKE_SOURCE_DIR}/external/godot-cpp)

include_directories(
        ${GODOT_CPP_PATH}/include
        ${GODOT_CPP_PATH}/gen/include
        ${GODOT_CPP_PATH}/gdextension
        ${CMAKE_SOURCE_DIR}/include
)

link_directories(${GODOT_CPP_PATH}/bin)

add_library(libopenshot_godot SHARED
        register_types.cpp
        osg_timeline.cpp
)

target_link_libraries(libopenshot_godot
        ${GODOT_CPP_PATH}/bin/libgodot-cpp.linux.template_debug.x86_64.a
        openshot
)

set_target_properties(libopenshot_godot PROPERTIES
        OUTPUT_NAME "libopenshot_godot"
        PREFIX ""
)
