set(PKG_CONFIG_USE_CMAKE_PREFIX_PATH ON)
find_package(PkgConfig)
pkg_check_modules(PC_BABL QUIET babl)

set(babl_VERSION ${PC_BABL_VERSION})

find_path(babl_INCLUDE_DIR
  NAMES babl/babl.h
  HINTS
    ${babl_DIR}/include
    ${PC_BABL_INCLUDE_DIRS}
  DOC "babl include dir"
)

find_library(babl_LIBRARY
  NAMES babl-0.1
  HINTS
    ${babl_DIR}/lib
    ${PC_BABL_LIBDIR}
    ${PC_BABL_LIBRARY_DIRS}
  DOC "babl library"
)

set ( babl_LIBRARIES ${babl_LIBRARY} )
set ( babl_INCLUDE_DIRS ${babl_INCLUDE_DIR} )

if (babl_INCLUDE_DIRS AND babl_LIBRARIES)
  set(babl_FOUND TRUE)
endif()

if(babl_FOUND AND NOT TARGET babl_lib)
	add_library(babl_lib UNKNOWN IMPORTED)

	set_property(TARGET babl_lib PROPERTY
		INTERFACE_INCLUDE_DIRECTORIES ${babl_INCLUDE_DIR})
	set_property(TARGET babl_lib PROPERTY
		IMPORTED_LOCATION ${babl_LIBRARY})
endif()

include ( FindPackageHandleStandardArgs )
# handle the QUIETLY and REQUIRED arguments and set babl_FOUND to TRUE
# if all listed variables are TRUE
find_package_handle_standard_args(babl
  REQUIRED_VARS
    babl_INCLUDE_DIR
    babl_LIBRARY
  VERSION_VAR
    babl_VERSION)
