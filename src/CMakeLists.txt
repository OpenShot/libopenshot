################ IMAGE MAGICK ##################
# Find the ImageMagick++ library
FIND_PACKAGE(ImageMagick REQUIRED COMPONENTS Magick++ MagickWand MagickCore)

# Include ImageMagick++ headers (needed for compile)
include_directories(${ImageMagick_INCLUDE_DIRS})

################### FFMPEG #####################
# Find FFmpeg libraries (used for video encoding / decoding)
FIND_PACKAGE(FFmpeg REQUIRED)

# Include FFmpeg headers (needed for compile)
include_directories(${FFMPEG_INCLUDE_DIR})

################# LIBOPENSHOT-AUDIO ###################
# Find JUCE-based openshot Audio libraries
FIND_PACKAGE(OpenShotAudio REQUIRED)

# Include Juce headers (needed for compile)
include_directories(${LIBJUCE_INCLUDE_DIRS})

################# SDL ###################
# Find SDL libraries
FIND_PACKAGE(SDL REQUIRED)

# Include SDL headers (needed for compile)
include_directories(${SDL_INCLUDE_DIR})

################# BLACKMAGIC DECKLINK ###################
# Find BlackMagic DeckLinkAPI libraries
FIND_PACKAGE(BlackMagic)

IF (BLACKMAGIC_FOUND)
	# Include headers (needed for compile)
	include_directories(${BLACKMAGIC_INCLUDE_DIR})
	
	# define a global var (used in the C++)
	add_definitions( -DUSE_BLACKMAGIC=1 )
	SET(CMAKE_SWIG_FLAGS "-DUSE_BLACKMAGIC=1")
	
ENDIF (BLACKMAGIC_FOUND)

################### OPENMP #####################
# Check for OpenMP (used for multi-core processing)
FIND_PACKAGE(OpenMP)

# Add the OpenMP compiler flag
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} ${OpenMP_CXX_FLAGS}")

###############  PROFILING  #################
#set(PROFILER "/usr/lib/libprofiler.so")
#set(PROFILER "/usr/lib/libtcmalloc.so")


###############  SET LIBRARY SOURCE FILES  #################
SET ( OPENSHOT_SOURCE_FILES
  		AudioBufferSource.cpp
		AudioResampler.cpp
		Cache.cpp
		Clip.cpp
		Coordinate.cpp
		DummyReader.cpp
		FileReaderBase.cpp
		FileWriterBase.cpp
		FFmpegReader.cpp
		FFmpegWriter.cpp
		Fraction.cpp
		Frame.cpp
		FrameMapper.cpp
		FrameRate.cpp
		KeyFrame.cpp
		ImageReader.cpp
		Player.cpp
		Point.cpp
		TextReader.cpp
		Timeline.cpp )
		
		IF (BLACKMAGIC_FOUND)
			SET ( OPENSHOT_SOURCE_FILES ${OPENSHOT_SOURCE_FILES} 
				DecklinkInput.cpp
				DecklinkReader.cpp
				DecklinkOutput.cpp
				DecklinkWriter.cpp)
		ENDIF (BLACKMAGIC_FOUND)
		

############### CREATE LIBRARY #################
# Create shared openshot library
add_library(openshot SHARED
			${OPENSHOT_SOURCE_FILES})
			
# Set SONAME and other library properties
set_target_properties(openshot
			PROPERTIES
			VERSION ${PROJECT_VERSION}
			SOVERSION ${SO_VERSION})
			
###############  LINK LIBRARY  #################
SET ( REQUIRED_LIBRARIES
  		${ImageMagick_LIBRARIES}
		${FFMPEG_LIBRARIES}
		${LIBJUCE_LIBRARIES}
		${SDL_LIBRARY}
		${PROFILER} )
		
		IF (BLACKMAGIC_FOUND)
			SET ( REQUIRED_LIBRARIES ${REQUIRED_LIBRARIES} ${BLACKMAGIC_LIBRARY_DIR} )
		ENDIF (BLACKMAGIC_FOUND)

# Link all referenced libraries
target_link_libraries(openshot ${REQUIRED_LIBRARIES})


############### TEST EXECUTABLE ################
# Create test executable
add_executable(example Main.cpp)

# Link test executable to the new library
target_link_libraries(example openshot ${PROFILER} )

############### TEST BLACKMAGIC CAPTURE APP ################
IF (BLACKMAGIC_FOUND)
	# Create test executable
	add_executable(blackmagic 
				   Main_Blackmagic.cpp)
	
	# Link test executable to the new library
	target_link_libraries(blackmagic openshot ${PROFILER} )
ENDIF (BLACKMAGIC_FOUND)

############### SWIG PYTHON BINDINGS ################
FIND_PACKAGE(SWIG 2.0 REQUIRED)
INCLUDE(${SWIG_USE_FILE})

FIND_PACKAGE(PythonLibs)
INCLUDE_DIRECTORIES(${PYTHON_INCLUDE_PATH})
INCLUDE_DIRECTORIES(${CMAKE_CURRENT_SOURCE_DIR})

SET_SOURCE_FILES_PROPERTIES(openshot.i PROPERTIES CPLUSPLUS ON)
SWIG_ADD_MODULE(openshot python openshot.i)
SWIG_LINK_LIBRARIES(openshot ${PYTHON_LIBRARIES} openshot)

############### INSTALL HEADERS & LIBRARY ################
# Headers
file(GLOB_RECURSE headers ${CMAKE_SOURCE_DIR}/include/*.h)
INSTALL(FILES ${headers} DESTINATION include/libopenshot)

# Install primary library
INSTALL(TARGETS openshot DESTINATION lib)

# Install Swig python library
INSTALL(TARGETS _openshot DESTINATION share/pyshared/libopenshot)

# Install Python bindings
INSTALL(FILES ${CMAKE_CURRENT_BINARY_DIR}/openshot.py DESTINATION share/pyshared/libopenshot)
INSTALL(FILES ${CMAKE_SOURCE_DIR}/cmake/Python/__init__.py DESTINATION share/pyshared/libopenshot)
