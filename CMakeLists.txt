cmake_minimum_required(VERSION 2.6)

################ ADD CMAKE MODULES ##################
set(CMAKE_MODULE_PATH "${CMAKE_SOURCE_DIR}/cmake/Modules")


################### SETUP PROJECT ###################
project(openshot)
set(PROJECT_VERSION 1.3.0)
MESSAGE("--------------------------------------------------------------")
message("---- Generating build files for ${PROJECT_NAME} (${PROJECT_VERSION})")
MESSAGE("--------------------------------------------------------------")

IF (WIN32)
	SET_PROPERTY(GLOBAL PROPERTY WIN32 "WIN32")
ENDIF(WIN32)

############## PROCESS SUB-DIRECTORIES ##############
add_subdirectory(src)
add_subdirectory(tests)


################### DOCUMENTATION ###################
# Find Doxygen (used for documentation)
include(cmake/Modules/UseDoxygen.cmake)