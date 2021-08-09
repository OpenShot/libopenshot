####################### CMakeLists.txt (libopenshot) #########################
# @brief CMake build file for libopenshot (used to generate makefiles)
# @author Jonathan Thomas <jonathan@openshot.org>
#
# @section LICENSE
#
# Copyright (c) 2008-2019 OpenShot Studios, LLC
#
# SPDX-License-Identifier: LGPL-3.0-or-later

IF(NOT DEFINED ENABLE_PYTHON)
    SET(ENABLE_PYTHON 1)
ENDIF()

IF(NOT DEFINED ENABLE_RUBY)
    SET(ENABLE_RUBY 1)
ENDIF()

############### INCLUDE EACH LANGUAGE BINDING ################
IF (ENABLE_PYTHON)
    add_subdirectory(python)
ENDIF (ENABLE_PYTHON)

IF (ENABLE_RUBY)
    add_subdirectory(ruby)
ENDIF (ENABLE_RUBY)

