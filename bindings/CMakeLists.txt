####################### CMakeLists.txt (libopenshot) #########################
# @brief CMake build file for libopenshot (used to generate makefiles)
# @author Jonathan Thomas <jonathan@openshot.org>
#
# @section LICENSE
#
# Copyright (c) 2008-2024 OpenShot Studios, LLC
#
# SPDX-License-Identifier: LGPL-3.0-or-later

IF(NOT DEFINED ENABLE_PYTHON)
    SET(ENABLE_PYTHON 1)
ENDIF()

IF(NOT DEFINED ENABLE_RUBY)
    SET(ENABLE_RUBY 1)
ENDIF()

IF(NOT DEFINED ENABLE_JAVA)
    SET(ENABLE_JAVA 0)
ENDIF()

IF(NOT DEFINED ENABLE_GODOT)
    SET(ENABLE_GODOT 0)
ENDIF()

############### INCLUDE EACH LANGUAGE BINDING ################
IF (ENABLE_PYTHON)
    add_subdirectory(python)
ENDIF (ENABLE_PYTHON)

IF (ENABLE_RUBY)
    add_subdirectory(ruby)
ENDIF (ENABLE_RUBY)

IF (ENABLE_JAVA)
    add_subdirectory(java)
ENDIF (ENABLE_JAVA)

IF (ENABLE_GODOT)
    add_subdirectory(godot)
ENDIF (ENABLE_GODOT)