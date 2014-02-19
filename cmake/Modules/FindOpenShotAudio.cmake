# - Try to find JUCE-based OpenShot Audio Library
# libopenshot-audio;
# Once done this will define
#  LIBOPENSHOT_AUDIO_FOUND - System has libjuce.so
#  LIBOPENSHOT_AUDIO_INCLUDE_DIRS - The juce.h include directories
#  LIBOPENSHOT_AUDIO_LIBRARIES - The libraries needed to use juce

message("$ENV{LIBOPENSHOT_AUDIO_DIR}")

# Find the base directory of juce includes
find_path(LIBOPENSHOT_AUDIO_BASE_DIR JuceHeader.h
			PATHS /usr/include/libopenshot-audio/
			/usr/local/include/libopenshot-audio/
			$ENV{LIBOPENSHOT_AUDIO_DIR}/include/libopenshot-audio/ )

# Get a list of all header file paths
FILE(GLOB_RECURSE JUCE_HEADER_FILES
  ${LIBOPENSHOT_AUDIO_BASE_DIR}/*.h
)

# Loop through each header file
FOREACH(HEADER_PATH ${JUCE_HEADER_FILES})
	# Get the directory of each header file
	get_filename_component(HEADER_DIRECTORY ${HEADER_PATH}
		PATH
	)
	
	# Append each directory into the HEADER_DIRECTORIES list
	LIST(APPEND HEADER_DIRECTORIES ${HEADER_DIRECTORY})
ENDFOREACH(HEADER_PATH)

# Remove duplicates from the header directories list
LIST(REMOVE_DUPLICATES HEADER_DIRECTORIES)

# Find the libopenshot-audio.so / libopenshot-audio.dll library
find_library(LIBOPENSHOT_AUDIO_LIBRARY
			NAMES libopenshot-audio openshot-audio
			HINTS /usr/lib/
			/usr/lib/libopenshot-audio/
			/usr/local/lib/
			$ENV{LIBOPENSHOT_AUDIO_DIR}/lib/ )

set(LIBOPENSHOT_AUDIO_LIBRARIES ${LIBOPENSHOT_AUDIO_LIBRARY})
set(LIBOPENSHOT_AUDIO_LIBRARY ${LIBOPENSHOT_AUDIO_LIBRARIES})

# Seems to work fine with just the base dir (rather than all the actual include folders)
set(LIBOPENSHOT_AUDIO_INCLUDE_DIR ${LIBOPENSHOT_AUDIO_BASE_DIR} )
set(LIBOPENSHOT_AUDIO_INCLUDE_DIRS ${LIBOPENSHOT_AUDIO_BASE_DIR} )

include(FindPackageHandleStandardArgs)
# handle the QUIETLY and REQUIRED arguments and set LIBOPENSHOT_AUDIO_FOUND to TRUE
# if all listed variables are TRUE
find_package_handle_standard_args(LIBOPENSHOT_AUDIO  DEFAULT_MSG
                                  LIBOPENSHOT_AUDIO_LIBRARY LIBOPENSHOT_AUDIO_INCLUDE_DIR)
