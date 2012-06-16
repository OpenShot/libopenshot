############### TEST EXECUTABLES ################

#add_executable(Example_FFmpeg Example_FFmpeg.cpp)
#target_link_libraries(Example_FFmpeg ${FFMPEG_LIBRARIES} ${ImageMagick_LIBRARIES} )

add_executable(Example_ImageMagick Example_ImageMagick.cpp)
target_link_libraries(Example_ImageMagick ${ImageMagick_LIBRARIES} )

#add_executable(Example_Juce Example_Juce.cpp)
#target_link_libraries(Example_Juce ${LIBJUCE_LIBRARIES})