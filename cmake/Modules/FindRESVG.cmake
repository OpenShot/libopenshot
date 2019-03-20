# - Try to find RESVG
# Once done this will define
# RESVG_FOUND - System has RESVG
# RESVG_INCLUDE_DIRS - The RESVG include directories
# RESVG_LIBRARIES - The libraries needed to use RESVG
find_path ( RESVG_INCLUDE_DIR ResvgQt.h
            PATHS ${RESVGDIR}/include/resvg
                  $ENV{RESVGDIR}/include/resvg
                  $ENV{RESVGDIR}/include
                  /usr/include/resvg
                  /usr/include
                  /usr/local/include/resvg
                  /usr/local/include )

find_library ( RESVG_LIBRARY NAMES resvg
               PATHS /usr/lib
                    /usr/local/lib
                    $ENV{RESVGDIR}
                    $ENV{RESVGDIR}/lib )

set ( RESVG_LIBRARIES ${RESVG_LIBRARY} )
set ( RESVG_INCLUDE_DIRS ${RESVG_INCLUDE_DIR} )

include ( FindPackageHandleStandardArgs )
# handle the QUIETLY and REQUIRED arguments and set RESVG_FOUND to TRUE
# if all listed variables are TRUE
find_package_handle_standard_args ( RESVG "Could NOT find RESVG, using Qt SVG parsing instead" RESVG_LIBRARY RESVG_INCLUDE_DIR )
mark_as_advanced( RESVG_LIBRARY RESVG_INCLUDE_DIR )
