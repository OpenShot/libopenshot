# Locate UNITTEST
# This module defines
# UNITTEST++_LIBRARY
# UNITTEST++_FOUND, if false, do not try to link to gdal
# UNITTEST++_INCLUDE_DIR, where to find the headers

FIND_PATH(UNITTEST++_INCLUDE_DIR UnitTest++.h
    HINTS
        ENV UNITTEST_DIR
    PATHS
        [HKEY_LOCAL_MACHINE\\SYSTEM\\CurrentControlSet\\Control\\Session\ Manager\\Environment]
        ~/Library/Frameworks
        /Library/Frameworks
        $ENV{UNITTEST_DIR}/src
    PATH_SUFFIXES
        include/unittest++
        include/UnitTest++
        include/unittest-cpp
        include
        unittest++
        UnitTest++
        unittest-cpp
)

FIND_LIBRARY(UNITTEST++_LIBRARY
    NAMES unittest++ UnitTest++
    HINTS
        ENV UNITTEST_DIR
    PATHS
        [HKEY_LOCAL_MACHINE\\SYSTEM\\CurrentControlSet\\Control\\Session\ Manager\\Environment]
        $ENV{UNITTEST_DIR}/build
        ~/Library/Frameworks
        /Library/Frameworks
    PATH_SUFFIXES
        lib
        lib64
)

SET(UNITTEST++_FOUND "NO")
IF(UNITTEST++_LIBRARY AND UNITTEST++_INCLUDE_DIR)
    SET(UNITTEST++_FOUND "YES")
ENDIF(UNITTEST++_LIBRARY AND UNITTEST++_INCLUDE_DIR)

include(FindPackageHandleStandardArgs)
# handle the QUIETLY and REQUIRED arguments and set UNITTEST++_FOUND to TRUE
# if all listed variables are TRUE
find_package_handle_standard_args(UNITTEST++  DEFAULT_MSG
                                  UNITTEST++_LIBRARY UNITTEST++_INCLUDE_DIR)
