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

################# JUCE AUDIO ###################
# Find JUCE Audio libraries
FIND_PACKAGE(Juce REQUIRED)

# Include Juce headers (needed for compile)
include_directories(${LIBJUCE_INCLUDE_DIRS})

################# SDL ###################
# Find SDL libraries
FIND_PACKAGE(SDL REQUIRED)

# Include SDL headers (needed for compile)
include_directories(${SDL_INCLUDE_DIR})

################### OPENMP #####################
# Check for OpenMP (used for multi-core processing)
FIND_PACKAGE(OpenMP)

# Add the OpenMP compiler flag
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} ${OpenMP_CXX_FLAGS}")


############### CREATE LIBRARY #################
# Create shared openshot library
add_library(openshot SHARED
			AudioBufferSource.cpp
			Cache.cpp
			Coordinate.cpp
			FFmpegReader.cpp
			FileReaderBase.cpp
			Fraction.cpp
			Frame.cpp
			FrameMapper.cpp
			FrameRate.cpp
			KeyFrame.cpp
			Player.cpp
			Point.cpp)

# Link all referenced libraries
target_link_libraries(openshot ${ImageMagick_LIBRARIES} ${FFMPEG_LIBRARIES} ${LIBJUCE_LIBRARIES} ${SDL_LIBRARY}) #${GTK2_LIBRARIES}


############### TEST EXECUTABLE ################
# Create test executable
add_executable(example Main.cpp)

# Link test executable to the new library
target_link_libraries(example openshot)

############### SWIG PYTHON BINDINGS ################
FIND_PACKAGE(SWIG REQUIRED)
INCLUDE(${SWIG_USE_FILE})

FIND_PACKAGE(PythonLibs)
INCLUDE_DIRECTORIES(${PYTHON_INCLUDE_PATH})
INCLUDE_DIRECTORIES(${CMAKE_CURRENT_SOURCE_DIR})

SET(CMAKE_SWIG_FLAGS "")

SET_SOURCE_FILES_PROPERTIES(openshot.i PROPERTIES CPLUSPLUS ON)
SWIG_ADD_MODULE(openshot python openshot.i)
SWIG_LINK_LIBRARIES(openshot ${PYTHON_LIBRARIES} openshot)

