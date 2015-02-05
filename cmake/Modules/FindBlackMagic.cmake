# - Try to find the BlackMagic DeckLinkAPI
# Once done this will define
#  
#  BLACKMAGIC_FOUND        - system has BlackMagic DeckLinkAPI installed
#  BLACKMAGIC_INCLUDE_DIR  - the include directory containing DeckLinkAPIDispatch.cpp
#  BLACKMAGIC_LIBRARY_DIR  - the directory containing libDeckLinkAPI.so
#   

# A user-defined environment variable is required to find the BlackMagic SDK
if(NOT EXISTS "$ENV{BLACKMAGIC_DIR}")
  message("-- Note: BLACKMAGIC_DIR environment variable is not defined")
endif(NOT EXISTS "$ENV{BLACKMAGIC_DIR}")

IF (WIN32)
	# WINDOWS
	FIND_PATH( BLACKMAGIC_INCLUDE_DIR DeckLinkAPI.h
			   PATHS $ENV{BLACKMAGIC_DIR}/Win/include/
			   		 "/home/jonathan/Blackmagic DeckLink SDK 10.3.1/Win/include/" )

	FIND_LIBRARY( BLACKMAGIC_LIBRARY_DIR DeckLinkAPI
			  PATHS /usr/lib/
					/usr/local/lib/
					$ENV{BLACKMAGIC_DIR}/lib/ )

ELSE (WIN32)
	IF   (UNIX)
		IF   (APPLE)
			# APPLE
			FIND_PATH( BLACKMAGIC_INCLUDE_DIR DeckLinkAPI.h
					   PATHS $ENV{BLACKMAGIC_DIR}/Mac/include/
					   		 "/home/jonathan/Blackmagic DeckLink SDK 10.3.1/Mac/include/" )

			FIND_LIBRARY( BLACKMAGIC_LIBRARY_DIR DeckLinkAPI
					  PATHS /usr/lib/
							/usr/local/lib/
							$ENV{BLACKMAGIC_DIR}/lib/ )

		ELSE (APPLE)
			# LINUX
			FIND_PATH( BLACKMAGIC_INCLUDE_DIR DeckLinkAPI.h
					   PATHS $ENV{BLACKMAGIC_DIR}/Linux/include/
					   		 "/home/jonathan/Blackmagic DeckLink SDK 10.3.1/Linux/include/" )

			FIND_LIBRARY( BLACKMAGIC_LIBRARY_DIR DeckLinkAPI
					  PATHS /usr/lib/
							/usr/local/lib/
							$ENV{BLACKMAGIC_DIR}/lib/ )

		ENDIF(APPLE)
	ENDIF(UNIX)
ENDIF(WIN32)

SET( BLACKMAGIC_FOUND FALSE )

IF ( BLACKMAGIC_INCLUDE_DIR AND BLACKMAGIC_LIBRARY_DIR )
    SET ( BLACKMAGIC_FOUND TRUE )
ENDIF ( BLACKMAGIC_INCLUDE_DIR AND BLACKMAGIC_LIBRARY_DIR )

MARK_AS_ADVANCED(
  BLACKMAGIC_INCLUDE_DIR
  BLACKMAGIC_LIBRARY_DIR
)

include(FindPackageHandleStandardArgs)
# handle the QUIETLY and REQUIRED arguments and set BLACKMAGIC_FOUND to TRUE
# if all listed variables are TRUE
find_package_handle_standard_args(BLACKMAGIC  DEFAULT_MSG
                                  BLACKMAGIC_LIBRARY_DIR BLACKMAGIC_INCLUDE_DIR)
