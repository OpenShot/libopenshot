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

# Dependencies
find_package(Protobuf 3 REQUIRED)

# Create a target for libprotobuf, if necessary (CMake < 3.9)
if(NOT TARGET protobuf::libprotobuf)
  add_library(protobuf_TARGET INTERFACE)
  target_include_directories(protobuf_TARGET
    INTERFACE ${Protobuf_INCLUDE_DIRS})
  target_link_libraries(protobuf_TARGET INTERFACE ${Protobuf_LIBRARIES})
  add_library(protobuf::libprotobuf ALIAS protobuf_TARGET)
endif()

file(GLOB ProtoFiles "${CMAKE_CURRENT_SOURCE_DIR}/*.proto")
PROTOBUF_GENERATE_CPP(ProtoSources ProtoHeaders ${ProtoFiles})
add_library(openshot_protobuf SHARED ${ProtoSources} ${ProtoHeaders})

set(ProtobufMessagePath ${CMAKE_CURRENT_BINARY_DIR}
    CACHE INTERNAL "Path to generated protobuf header files.")

target_link_libraries(openshot_protobuf protobuf::libprotobuf)

# Set SONAME and other library properties
set_target_properties(openshot_protobuf PROPERTIES
        VERSION ${PROJECT_VERSION}
        SOVERSION ${PROJECT_SO_VERSION}
        INSTALL_NAME_DIR "${CMAKE_INSTALL_PREFIX}/lib"
        )

# Install protobuf library and generated headers
install(TARGETS openshot_protobuf
  COMPONENT runtime
  RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
  LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
  ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR}
  INCLUDES DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}/libopenshot
  )
install(FILES ${ProtoHeaders}
  COMPONENT devel
  DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}/libopenshot/protobuf_messages
)

# On Windows, we copy project output DLLs into the test dirs
# so that the unit test executables can find them
if(CMAKE_VERSION VERSION_GREATER 3.13 AND WIN32 AND BUILD_TESTING)
  add_custom_command(TARGET openshot_protobuf POST_BUILD
    COMMAND
      ${CMAKE_COMMAND} -E copy_if_different
      "$<TARGET_FILE:openshot_protobuf>"
      "${PROJECT_BINARY_DIR}/tests/"
    BYPRODUCTS
      "${PROJECT_BINARY_DIR}/tests/libopenshot_protobuf.dll"
    COMMENT
      "Copying libopenshot_protobuf DLL to unit test directory"
  )
endif()
