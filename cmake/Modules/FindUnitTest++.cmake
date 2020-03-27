# Locate UnitTest++
# This module defines
# UnitTest++_FOUND, if successful
# UnitTest++_LIBRARIES, the library path
# UnitTest++_INCLUDE_DIRS, where to find the headers

find_package(PkgConfig QUIET)
if(PKG_CONFIG_FOUND)
  pkg_check_modules(PC_UnitTest QUIET UnitTest++)
  set(UnitTest++_VERSION ${PC_UnitTest_VERSION})
endif()


FIND_PATH(UnitTest++_INCLUDE_DIRS UnitTest++.h
  DOC
    "Location of UnitTest++ header files"
  PATH_SUFFIXES
    unittest++
    UnitTest++  # Fedora, Arch
    unittest-cpp  # openSUSE
  HINTS
    ${PC_UnitTest++_INCLUDEDIR}
    ${PC_UnitTest++_INCLUDE_DIRS}
  PATHS
    ${UnitTest++_ROOT}
    ${UNITTEST_DIR}
    $ENV{UNITTEST_DIR}/src
    $ENV{UNITTEST_DIR}
    ~/Library/Frameworks
    /Library/Frameworks
    /usr/local
    /sw  # Fink
    /opt
    /opt/local  # DarwinPorts
    /opt/csw  # Blastwave
    [HKEY_LOCAL_MACHINE\\SYSTEM\\CurrentControlSet\\Control\\Session\ Manager\\Environment]/include
    /usr/freeware
)

FIND_LIBRARY(UnitTest++_LIBRARIES 
  NAMES unittest++ UnitTest++
  DOC
    "Location of UnitTest++ shared library"
  HINTS
    ${PC_UnitTest++_LIBDIR}
    ${PC_UnitTest++_LIBRARY_DIRS}
  PATHS
    ${UnitTest++_ROOT}
    ${UnitTest++_ROOT}/lib
    ${UNITTEST_DIR}
    $ENV{UNITTEST_DIR}
    $ENV{UNITTEST_DIR}/lib
    $ENV{UNITTEST_DIR}/build
    ~/Library/Frameworks
    /Library/Frameworks
    /usr/local/lib
    /sw/lib
    /opt/local/lib
    /opt/csw/lib
    /opt/lib
    [HKEY_LOCAL_MACHINE\\SYSTEM\\CurrentControlSet\\Control\\Session\ Manager\\Environment]/lib
    /usr/freeware/lib64
)

if(UnitTest++_LIBRARIES AND UnitTest++_INCLUDE_DIRS)
  set(UnitTest++_FOUND TRUE)
endif()

include(FindPackageHandleStandardArgs)
find_package_handle_standard_args(UnitTest++
  REQUIRED_VARS
    UnitTest++_LIBRARIES
    UnitTest++_INCLUDE_DIRS
  VERSION_VAR
    UnitTest++_VERSION
)

# Excessive backwards-compatibility paranoia
set(UnitTest++_LIBRARY "${UnitTest++_LIBRARIES}" PARENT_SCOPE)
set(UnitTest++_INCLUDE_DIR "${UnitTest++_INCLUDE_DIRS}" PARENT_SCOPE)
# Even more excessive backwards-compatibility paranoia
set(UNITTEST++_FOUND "${UnitTest++_FOUND}" PARENT_SCOPE)
set(UNITTEST++_LIBRARY "${UnitTest++_LIBRARIES}" PARENT_SCOPE)
set(UNITTEST++_INCLUDE_DIR "${UnitTest++_INCLUDE_DIRS}" PARENT_SCOPE)

