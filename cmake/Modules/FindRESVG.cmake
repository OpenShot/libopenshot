# - Try to find RESVG
# Once done this will define
# RESVG_FOUND - System has RESVG
# RESVG_INCLUDE_DIRS - The RESVG include directories
# RESVG_LIBRARIES - The libraries needed to use RESVG
# RESVG_DEFINITIONS - Compiler switches required for using RESVG
find_path ( RESVG_INCLUDE_DIR ResvgQt.h
            PATHS ${RESVGDIR}/include/resvg/
                  $ENV{RESVGDIR}/include/resvg/
                  $ENV{RESVGDIR}/include/
                  /usr/include/
                  /usr/include/resvg/
                  /usr/local/include/
                  /usr/local/include/resvg/
                   )

find_library ( RESVG_LIBRARY NAMES resvg
               PATHS /usr/lib/
                    /usr/local/lib/
                    $ENV{RESVGDIR}/lib/ )

set ( RESVG_LIBRARIES ${RESVG_LIBRARY} )
set ( RESVG_INCLUDE_DIRS ${RESVG_INCLUDE_DIR} )

SET( RESVG_FOUND FALSE )

IF ( RESVG_INCLUDE_DIR AND RESVG_LIBRARY )
    SET ( RESVG_FOUND TRUE )

    MESSAGE('-- RESVG_INCLUDE_DIRS: ${RESVG_INCLUDE_DIRS}')
    MESSAGE('-- RESVG_LIBRARIES: ${RESVG_LIBRARIES}')

    include ( FindPackageHandleStandardArgs )
    # handle the QUIETLY and REQUIRED arguments and set RESVG_FOUND to TRUE
    # if all listed variables are TRUE
    find_package_handle_standard_args ( RESVG DEFAULT_MSG RESVG_LIBRARY RESVG_INCLUDE_DIR )
ENDIF ( RESVG_INCLUDE_DIR AND RESVG_LIBRARY )

