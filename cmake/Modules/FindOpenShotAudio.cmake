# - Try to find JUCE-based OpenShot Audio Library
# libopenshot-audio;
# Once done this will define
#  LIBOPENSHOT_AUDIO_FOUND - System has libjuce.so
#  LIBOPENSHOT_AUDIO_INCLUDE_DIRS - The juce.h include directories
#  LIBOPENSHOT_AUDIO_LIBRARIES - The libraries needed to use juce

message("$ENV{LIBOPENSHOT_AUDIO_DIR}")

# Find the libopenshot-audio header files
find_path(LIBOPENSHOT_AUDIO_INCLUDE_DIR JuceHeader.h
			PATHS $ENV{LIBOPENSHOT_AUDIO_DIR}/include/libopenshot-audio/
			/usr/include/libopenshot-audio/
			/usr/local/include/libopenshot-audio/ )

# Find the libopenshot-audio.so (check env var first)
find_library(LIBOPENSHOT_AUDIO_LIBRARY
		NAMES libopenshot-audio openshot-audio
		PATHS $ENV{LIBOPENSHOT_AUDIO_DIR}/lib/ NO_DEFAULT_PATH)

# Find the libopenshot-audio.so / libopenshot-audio.dll library (fallback)
find_library(LIBOPENSHOT_AUDIO_LIBRARY
			NAMES libopenshot-audio openshot-audio
			HINTS $ENV{LIBOPENSHOT_AUDIO_DIR}/lib/
			/usr/lib/
			/usr/lib/libopenshot-audio/
			/usr/local/lib/ )

set(LIBOPENSHOT_AUDIO_LIBRARIES ${LIBOPENSHOT_AUDIO_LIBRARY})
set(LIBOPENSHOT_AUDIO_LIBRARY ${LIBOPENSHOT_AUDIO_LIBRARIES})

set(LIBOPENSHOT_AUDIO_INCLUDE_DIRS ${LIBOPENSHOT_AUDIO_INCLUDE_DIR} )

include(FindPackageHandleStandardArgs)
# handle the QUIETLY and REQUIRED arguments and set LIBOPENSHOT_AUDIO_FOUND to TRUE
# if all listed variables are TRUE
find_package_handle_standard_args(LIBOPENSHOT_AUDIO  DEFAULT_MSG
                                  LIBOPENSHOT_AUDIO_LIBRARY LIBOPENSHOT_AUDIO_INCLUDE_DIR)
