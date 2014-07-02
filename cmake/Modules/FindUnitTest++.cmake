# Locate UNITTEST
# This module defines
# UNITTEST++_LIBRARY
# UNITTEST++_FOUND, if false, do not try to link to gdal 
# UNITTEST++_INCLUDE_DIR, where to find the headers

FIND_PATH(UNITTEST++_INCLUDE_DIR UnitTest++.h
    ${UNITTEST_DIR}/include/unittest++
    $ENV{UNITTEST_DIR}/include/unittest++
	$ENV{UNITTEST_DIR}/src
    $ENV{UNITTEST_DIR}
    ~/Library/Frameworks
    /Library/Frameworks
    /usr/local/include
    /usr/include
    /usr/include/unittest++
    /usr/include/UnitTest++ # Fedora
    /usr/include/unittest-cpp # openSUSE
    /usr/local/include/UnitTest++/ # Arch
    /sw/include # Fink
    /opt/local/include # DarwinPorts
    /opt/local/include/UnitTest++
    /opt/csw/include # Blastwave
    /opt/include
    [HKEY_LOCAL_MACHINE\\SYSTEM\\CurrentControlSet\\Control\\Session\ Manager\\Environment]/include
    /usr/freeware/include
)

FIND_LIBRARY(UNITTEST++_LIBRARY 
    NAMES unittest++ UnitTest++
    PATHS
    ${UNITTEST_DIR}/lib
    $ENV{UNITTEST_DIR}/lib
	$ENV{UNITTEST_DIR}/build
    $ENV{UNITTEST_DIR}
    ~/Library/Frameworks
    /Library/Frameworks
    /usr/local/lib
    /usr/lib
    /usr/lib64/ # Fedora
    /sw/lib
    /opt/local/lib
    /opt/csw/lib
    /opt/lib
    [HKEY_LOCAL_MACHINE\\SYSTEM\\CurrentControlSet\\Control\\Session\ Manager\\Environment]/lib
    /usr/freeware/lib64
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
