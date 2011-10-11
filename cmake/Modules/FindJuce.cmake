# - Try to find JUCE Audio Libraries
# libjuce;
# Once done this will define
#  LIBJUCE_FOUND - System has libjuce.so
#  LIBJUCE_INCLUDE_DIRS - The juce.h include directories
#  LIBJUCE_LIBRARIES - The libraries needed to use juce

# Find the base directory of juce includes
find_path(LIBJUCE_BASE_DIR juce_amalgamated.h
			PATHS /usr/include/juce/
			/usr/local/include/juce/
			$ENV{JUCE_DIR}/include/juce/
			$ENV{JUCE_DIR}/includes/juce/ )

# Get a list of all header file paths
FILE(GLOB_RECURSE JUCE_HEADER_FILES
  ${LIBJUCE_BASE_DIR}/*.h
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

# Find the juce.so / juce.dll library
find_library(LIBJUCE_LIBRARY
			NAMES libjuce juce
			HINTS /usr/lib/
			/usr/lib/juce/
			/usr/local/lib/
			$ENV{JUCE_DIR}/lib/ )

set(LIBJUCE_LIBRARIES ${LIBJUCE_LIBRARY})
set(LIBJUCE_LIBRARY ${LIBJUCE_LIBRARIES})
#set(LIBJUCE_INCLUDE_DIR ${HEADER_DIRECTORIES} )
#set(LIBJUCE_INCLUDE_DIRS ${LIBJUCE_INCLUDE_DIR} )

# Seems to work fine with just the base dir (rather than all the actual include folders)
set(LIBJUCE_INCLUDE_DIR ${LIBJUCE_BASE_DIR} )
set(LIBJUCE_INCLUDE_DIRS ${LIBJUCE_BASE_DIR} )



include(FindPackageHandleStandardArgs)
# handle the QUIETLY and REQUIRED arguments and set LIBJUCE_FOUND to TRUE
# if all listed variables are TRUE
find_package_handle_standard_args(LIBJUCE  DEFAULT_MSG
                                  LIBJUCE_LIBRARY LIBJUCE_INCLUDE_DIR)
