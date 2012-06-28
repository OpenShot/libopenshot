################### UNITTEST++ #####################
# Find UnitTest++ libraries (used for unit testing)
FIND_PACKAGE(UnitTest++ REQUIRED)

# Include UnitTest++ headers (needed for compile)
include_directories(${UNITTEST++_INCLUDE_DIR})

################ IMAGE MAGICK ##################
# Find the ImageMagick++ library
FIND_PACKAGE(ImageMagick COMPONENTS Magick++ MagickWand MagickCore)

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

################ TESTER EXECUTABLE #################
# Create unit test executable (tester)
add_executable(tester
			   tests.cpp
			   Cache_Tests.cpp
			   Coordinate_Tests.cpp
			   FileReaderBase_Tests.cpp
			   Fraction_Tests.cpp
			   FrameMapper_Tests.cpp
			   FrameRate_Tests.cpp
			   KeyFrame_Tests.cpp
			   Point_Tests.cpp)

# Link libraries to the new executable
target_link_libraries(tester openshot ${UNITTEST++_LIBRARY})


#################### MAKE TEST ######################
# Hook up the 'make test' target to the 'tester' executable
ADD_CUSTOM_TARGET(test ${CMAKE_CURRENT_BINARY_DIR}/tester)