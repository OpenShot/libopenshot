####################### CMakeLists.txt (libopenshot) #########################
# @brief CMake build file for libopenshot (used to generate Java SWIG bindings)
# @author Jonathan Thomas <jonathan@openshot.org>
#
# @section LICENSE
#
# Copyright (c) 2008-2024 OpenShot Studios, LLC
#
# SPDX-License-Identifier: LGPL-3.0-or-later

############### JAVA BINDINGS ################
find_package(SWIG 4.0 REQUIRED)
include(${SWIG_USE_FILE})

### Use modern SWIG target behavior when available
if (POLICY CMP0078)
    cmake_policy(SET CMP0078 NEW)
endif()
if (POLICY CMP0086)
    cmake_policy(SET CMP0086 NEW)
endif()

find_package(Java REQUIRED)
include(UseJava)

find_package(JNI REQUIRED)
include_directories(${JNI_INCLUDE_DIRS})

### Enable C++ in SWIG
set_property(SOURCE openshot.i PROPERTY CPLUSPLUS ON)
set_property(SOURCE openshot.i PROPERTY SWIG_MODULE_NAME openshot)

### Set the swig package name for the JAR
set_source_files_properties(openshot.i PROPERTIES
        SWIG_FLAGS "-package;org.openshot;-w503,516,813,822"
)

### Suppress a ton of warnings in the generated SWIG C++ code
set(SWIG_CXX_FLAGS "-Wno-unused-variable -Wno-unused-function \
  -Wno-deprecated-copy -Wno-class-memaccess -Wno-cast-function-type \
  -Wno-unused-parameter -Wno-catch-value -Wno-sign-compare -Wno-ignored-qualifiers")
separate_arguments(sw_flags UNIX_COMMAND ${SWIG_CXX_FLAGS})
set_property(SOURCE openshot.i PROPERTY GENERATED_COMPILE_OPTIONS ${sw_flags})

### Take include dirs from target
if(CMAKE_VERSION VERSION_GREATER 3.15)
    set(_inc $<REMOVE_DUPLICATES:$<TARGET_PROPERTY:openshot,INCLUDE_DIRECTORIES>>)
elseif(CMAKE_VERSION VERSION_GREATER 3.12)
    set(_inc $<TARGET_PROPERTY:openshot,INCLUDE_DIRECTORIES>)
endif()
if (DEFINED _inc)
    set_property(SOURCE openshot.i PROPERTY INCLUDE_DIRECTORIES ${_inc})
endif()

### Properly manage dependencies (regenerate bindings after changes)
if (CMAKE_VERSION VERSION_GREATER 3.20)
    set_property(SOURCE openshot.i PROPERTY USE_SWIG_DEPENDENCIES TRUE)
endif()

### Add the SWIG interface file (which defines all the SWIG methods)
swig_add_library(openshot-java LANGUAGE java SOURCES openshot.i)

if (POLICY CMP0078)
    set(_openshot_java_target openshot-java)
else()
    set(_openshot_java_target ${SWIG_MODULE_openshot-java_REAL_NAME})
endif()

### Set the prefix for the library to 'lib'
#set_target_properties(${SWIG_MODULE_openshot-java_REAL_NAME} PROPERTIES PREFIX "lib")

### Compile the generated wrapper file
get_property(_java_files TARGET openshot-java PROPERTY SWIG_SUPPORT_FILES)

### Link the new Java wrapper library with libopenshot
target_link_libraries(${_openshot_java_target} PUBLIC
        ${JNI_LIBRARIES} openshot)

### Create a custom target for the JAR file
add_custom_command(
        OUTPUT ${CMAKE_BINARY_DIR}/bindings/java/openshotJNI.jar
        COMMAND ${Java_JAVAC_EXECUTABLE} -d ${CMAKE_BINARY_DIR}/bindings/java ${CMAKE_BINARY_DIR}/bindings/java/*.java
        COMMAND ${Java_JAR_EXECUTABLE} -cf ${CMAKE_BINARY_DIR}/bindings/java/openshotJNI.jar -C ${CMAKE_BINARY_DIR}/bindings/java .
        DEPENDS ${_openshot_java_target}
        WORKING_DIRECTORY ${CMAKE_BINARY_DIR}/bindings/java
        COMMENT "Creating openshotJNI.jar"
)

add_custom_target(openshotJNIJar ALL DEPENDS ${CMAKE_BINARY_DIR}/bindings/java/openshotJNI.jar)
