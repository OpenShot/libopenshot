# - Try to find JsonCpp
#
# Once done this will define
#  JSONCPP_INCLUDE_DIRS, where to find header, etc.
#  JSONCPP_LIBRARIES, the libraries needed to use jsoncpp.
#  JSONCPP_FOUND, If false, do not try to use jsoncpp.
#  JSONCPP_INCLUDE_PREFIX, include prefix for jsoncpp

# try to detect using pkg-config, and use as hints later
find_package(PkgConfig)
pkg_check_modules(PC_jsoncpp QUIET jsoncpp)

find_path(
	JSONCPP_INCLUDE_DIR
	NAMES json/json.h
	HINTS ${PC_jsoncpp_INCLUDE_DIRS}
	DOC "jsoncpp include dir"
)

find_library(
	JSONCPP_LIBRARY
	NAMES jsoncpp
	HINTS ${PC_jsoncpp_LIBRARY_DIR}
	DOC "jsoncpp library"
)

set(JSONCPP_INCLUDE_DIRS ${JSONCPP_INCLUDE_DIR})
set(JSONCPP_LIBRARIES ${JSONCPP_LIBRARY})

# debug library on windows
# same naming convention as in qt (appending debug library with d)
# boost is using the same "hack" as us with "optimized" and "debug"
if ("${CMAKE_CXX_COMPILER_ID}" STREQUAL "MSVC")
	find_library(
		JSONCPP_LIBRARY_DEBUG
		NAMES jsoncppd
		HINTS ${PC_jsoncpp_LIBDIR} ${PC_jsoncpp_LIBRARY_DIRS}
		DOC "jsoncpp debug library"
	)

	set(JSONCPP_LIBRARIES optimized ${JSONCPP_LIBRARIES} debug ${JSONCPP_LIBRARY_DEBUG})

endif()

# find JSONCPP_INCLUDE_PREFIX
find_path(
	JSONCPP_INCLUDE_PREFIX
	NAMES json.h
	PATH_SUFFIXES jsoncpp/json json
)

if (${JSONCPP_INCLUDE_PREFIX} MATCHES "jsoncpp")
	set(JSONCPP_INCLUDE_PREFIX "jsoncpp/json")
else()
	set(JSONCPP_INCLUDE_PREFIX "json")
endif()

# handle the QUIETLY and REQUIRED arguments and set JSONCPP_FOUND to TRUE
# if all listed variables are TRUE, hide their existence from configuration view
include(FindPackageHandleStandardArgs)
find_package_handle_standard_args(jsoncpp DEFAULT_MSG
	JSONCPP_INCLUDE_DIR JSONCPP_LIBRARY)
mark_as_advanced (JSONCPP_INCLUDE_DIR JSONCPP_LIBRARY)

