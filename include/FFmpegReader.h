#ifndef OPENSHOT_FFMPEG_READER_BASE_H
#define OPENSHOT_FFMPEG_READER_BASE_H

/**
 * \file
 * \brief Header file for FFmpegReader class
 * \author Copyright (c) 2011 Jonathan Thomas
 */

#include "FileReaderBase.h"

// Required for libavformat to build on Windows
#ifndef INT64_C
#define INT64_C(c) (c ## LL)
#define UINT64_C(c) (c ## ULL)
#endif

extern "C" {
	#include <libavcodec/avcodec.h>
	#include <libavformat/avformat.h>
	#include <libswscale/swscale.h>
}

#include <iostream>
#include <stdio.h>
#include <ctime>
#include <omp.h>
#include "Magick++.h"
#include "Cache.h"
#include "Exceptions.h"

using namespace std;

namespace openshot
{

	/**
	 * \brief This class uses the FFmpeg libraries, to open video files and audio files, and return
	 * openshot::Frame objects for any frame in the file.
	 *
	 * All seeking and caching is handled internally, and the only public interface is the GetFrame()
	 * method.  To use this reader, simply create an instance of this class, and call the GetFrame method
	 * to start retrieving frames.  Use the info struct to obtain info on the file, such as the length
	 * (in frames), height, width, bitrate, frames per second (fps), etc...
	 */
	class FFmpegReader : public FileReaderBase
	{
	private:
		string path;

		AVFormatContext *pFormatCtx;
		int i, videoStream, audioStream;
		AVCodecContext *pCodecCtx, *aCodecCtx;
		AVCodec *pCodec, *aCodec;
		AVStream *pStream, *aStream;
		AVPacket packet;
		AVFrame *pFrame;

		Frame new_frame;
		Cache cache;
		bool is_seeking;
		int seeking_pts;

		bool found_pts_offset;
		int pts_offset;

		bool found_frame;
		int current_frame;
		int current_pts;
		int audio_position;
		bool needs_packet;

		/// Open File - which is called by the constructor automatically
		void Open();

		/// Convert image to RGB format
		Frame convert_image(AVPicture *copyFrame, int original_width, int original_height, PixelFormat pix_fmt);

		/// Convert PTS into Frame Number
		int ConvertPTStoFrame(int pts);

		/// Convert Frame Number into PTS
		int ConvertFrameToPTS(int frame_number);

	public:
		/// Constructor for FFmpegReader.  This automatically opens the media file and loads
		/// frame 1, or it throws one of the following exceptions.
		FFmpegReader(string path) throw(InvalidFile, NoStreamsFound, InvalidCodec);

		/// Close File
		void Close();

		/// Update File Info for video streams
		void UpdateVideoInfo();

		/// Update File Info for audio streams
		void UpdateAudioInfo();

		/// Get an openshot::Frame object for a specific frame number of this reader.
		///
		/// @returns The requested frame of video
		/// @param[in] number The frame number that is requested.
		Frame GetFrame(int requested_frame);

		/// Seek to a specific Frame.  This is not always frame accurate, it's more of an estimation on many codecs.
		void Seek(int requested_frame);

		/// Read the stream until we find the requested Frame
		Frame ReadStream(int requested_frame);

		/// Process a video packet
		void ProcessVideoPacket(int requested_frame);

		/// Process an audio packet
		void ProcessAudioPacket(int requested_frame);

		/// Get the next packet (if any)
		int GetNextPacket();

		/// Set the frame number and current pts
		void SetFrameNumber();

		/// Get an AVFrame (if any)
		bool GetAVFrame();

		/// Check the current seek position and determine if we need to seek again
		bool CheckSeek();

	};

}

#endif
