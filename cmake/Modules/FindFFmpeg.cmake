# - Try to find FFMPEG
# Once done this will define
#  
#  FFMPEG_FOUND        - system has FFMPEG
#  FFMPEG_INCLUDE_DIR  - the include directory
#  FFMPEG_LIBRARY_DIR  - the directory containing the libraries
#  FFMPEG_LIBRARIES    - Link these to use FFMPEG
#   

# FindAvformat
FIND_PATH( AVFORMAT_INCLUDE_DIR libavformat/avformat.h
		   PATHS /usr/include/
				 /usr/include/ffmpeg/
				 $ENV{FFMPEGDIR}/include/
				 $ENV{FFMPEGDIR}/include/ffmpeg/ )

FIND_LIBRARY( AVFORMAT_LIBRARY avformat
			  PATHS /usr/lib/
					/usr/lib/ffmpeg/
					$ENV{FFMPEGDIR}/lib/
					$ENV{FFMPEGDIR}/lib/ffmpeg/ )
#FindAvcodec
FIND_PATH( AVCODEC_INCLUDE_DIR libavcodec/avcodec.h
		   PATHS /usr/include/
				 /usr/include/ffmpeg/
				 $ENV{FFMPEGDIR}/include/
				 $ENV{FFMPEGDIR}/include/ffmpeg/ )

FIND_LIBRARY( AVCODEC_LIBRARY avcodec
			  PATHS /usr/lib/
					/usr/lib/ffmpeg/
					$ENV{FFMPEGDIR}/lib/
					$ENV{FFMPEGDIR}/lib/ffmpeg/ )
#FindAvutil
FIND_PATH( AVUTIL_INCLUDE_DIR libavutil/avutil.h
		   PATHS /usr/include/
				 /usr/include/ffmpeg/
				 $ENV{FFMPEGDIR}/include/
				 $ENV{FFMPEGDIR}/include/ffmpeg/ )

FIND_LIBRARY( AVUTIL_LIBRARY avutil
			  PATHS /usr/lib/
					/usr/lib/ffmpeg/
					$ENV{FFMPEGDIR}/lib/
					$ENV{FFMPEGDIR}/lib/ffmpeg/ )

#FindAvdevice
FIND_PATH( AVDEVICE_INCLUDE_DIR libavdevice/avdevice.h
		   PATHS /usr/include/
				 /usr/include/ffmpeg/
				 $ENV{FFMPEGDIR}/include/
				 $ENV{FFMPEGDIR}/include/ffmpeg/ )

FIND_LIBRARY( AVDEVICE_LIBRARY avdevice
			  PATHS /usr/lib/
					/usr/lib/ffmpeg/
					$ENV{FFMPEGDIR}/lib/
					$ENV{FFMPEGDIR}/lib/ffmpeg/ )
#FindSwscale
FIND_PATH( SWSCALE_INCLUDE_DIR libswscale/swscale.h
		   PATHS /usr/include/
				 /usr/include/ffmpeg/
				 $ENV{FFMPEGDIR}/include/
				 $ENV{FFMPEGDIR}/include/ffmpeg/ )

FIND_LIBRARY( SWSCALE_LIBRARY swscale
			  PATHS /usr/lib/
					/usr/lib/ffmpeg/
					$ENV{FFMPEGDIR}/lib/
					$ENV{FFMPEGDIR}/lib/ffmpeg/ )

SET( FFMPEG_FOUND FALSE )

IF ( AVFORMAT_INCLUDE_DIR AND AVFORMAT_LIBRARY )
    SET ( AVFORMAT_FOUND TRUE )
ENDIF ( AVFORMAT_INCLUDE_DIR AND AVFORMAT_LIBRARY )

IF ( AVCODEC_INCLUDE_DIR AND AVCODEC_LIBRARY ) 
    SET ( AVCODEC_FOUND TRUE)
ENDIF ( AVCODEC_INCLUDE_DIR AND AVCODEC_LIBRARY )

IF ( AVUTIL_INCLUDE_DIR AND AVUTIL_LIBRARY )
    SET ( AVUTIL_FOUND TRUE )
ENDIF ( AVUTIL_INCLUDE_DIR AND AVUTIL_LIBRARY )

IF ( AVDEVICE_INCLUDE_DIR AND AVDEVICE_LIBRARY ) 
    SET ( AVDEVICE_FOUND TRUE )
ENDIF ( AVDEVICE_INCLUDE_DIR AND AVDEVICE_LIBRARY )

IF ( SWSCALE_INCLUDE_DIR AND SWSCALE_LIBRARY )
    SET ( SWSCALE_FOUND TRUE )
ENDIF ( SWSCALE_INCLUDE_DIR AND SWSCALE_LIBRARY )


IF ( AVFORMAT_INCLUDE_DIR OR AVCODEC_INCLUDE_DIR OR AVUTIL_INCLUDE_DIR OR AVDEVICE_FOUND OR SWSCALE_FOUND )

	SET ( FFMPEG_FOUND TRUE )

	SET ( FFMPEG_INCLUDE_DIR
		  ${AVFORMAT_INCLUDE_DIR}
		  ${AVCODEC_INCLUDE_DIR}
		  ${AVUTIL_INCLUDE_DIR}
		  ${AVDEVICE_INCLUDE_DIR}
		  ${SWSCALE_INCLUDE_DIR} )
	
	SET ( FFMPEG_LIBRARIES 
		  ${AVFORMAT_LIBRARY}
		  ${AVCODEC_LIBRARY}
		  ${AVUTIL_LIBRARY}
		  ${AVDEVICE_LIBRARY}
		  ${SWSCALE_LIBRARY} )

ENDIF ( AVFORMAT_INCLUDE_DIR OR AVCODEC_INCLUDE_DIR OR AVUTIL_INCLUDE_DIR OR AVDEVICE_FOUND OR SWSCALE_FOUND )

MARK_AS_ADVANCED(
  FFMPEG_LIBRARY_DIR
  FFMPEG_INCLUDE_DIR
)

include(FindPackageHandleStandardArgs)
# handle the QUIETLY and REQUIRED arguments and set FFMPEG_FOUND to TRUE
# if all listed variables are TRUE
find_package_handle_standard_args(FFMPEG  DEFAULT_MSG
                                  FFMPEG_LIBRARIES FFMPEG_INCLUDE_DIR)