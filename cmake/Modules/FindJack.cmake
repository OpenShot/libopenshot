# - Try to find Jack Audio Libraries
# libjack; libjackserver;
# Once done this will define
#  LIBJACK_FOUND - System has libjack.so
#  LIBJACK_INCLUDE_DIRS - The jack.h include directories
#  LIBJACK_LIBRARIES - The libraries needed to use jack

find_path(LIBJACK_INCLUDE_DIR jack/jack.h
		  PATHS /usr/include/
				$ENV{JACK_DIR}/include/
			    $ENV{JACK_DIR}/includes/ )

find_library(LIBJACK_libjack_LIBRARY
			 NAMES libjack jack
             HINTS /usr/lib/
				   /usr/lib/jack/
				   $ENV{JACK_DIR}/lib/ )

find_library(LIBJACK_libjackserver_LIBRARY
			 NAMES libjackserver jackserver
             HINTS /usr/lib/
				   /usr/lib/jack/
				   $ENV{JACK_DIR}/lib/ )

set(LIBJACK_LIBRARIES ${LIBJACK_libjack_LIBRARY} ${LIBJACK_libjackserver_LIBRARY} )
set(LIBJACK_LIBRARY ${LIBJACK_LIBRARIES})
set(LIBJACK_INCLUDE_DIRS ${LIBJACK_INCLUDE_DIR} )

include(FindPackageHandleStandardArgs)
# handle the QUIETLY and REQUIRED arguments and set LIBJACK_FOUND to TRUE
# if all listed variables are TRUE
find_package_handle_standard_args(LIBJACK  DEFAULT_MSG
                                  LIBJACK_LIBRARY LIBJACK_INCLUDE_DIR)
