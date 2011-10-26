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
#include <cmath>
#include <ctime>
#include <iostream>
#include <omp.h>
#include <stdio.h>
#include "Magick++.h"
#include "Cache.h"
#include "Exceptions.h"


using namespace std;

namespace openshot
{
	/// This struct holds the associated video frame and starting sample # for an audio packet.
	/// Because audio packets do not match up with video frames, this helps determine exactly
	/// where the audio packet's samples belong.
	struct audio_packet_location
	{
		int frame;
		int sample_start;
	};

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

		Cache final_cache;
		Cache working_cache;

		bool is_seeking;
		int seeking_pts;
		int seeking_frame;
		bool is_video_seek;

		int audio_pts_offset;
		int video_pts_offset;

		int last_video_frame;
		int last_audio_frame;


		/// Open File - which is called by the constructor automatically
		void Open();

		/// Convert image to RGB format
		void convert_image(int current_frame, AVPicture *copyFrame, int width, int height, PixelFormat pix_fmt);

		/// Get the PTS for the current video packet
		int GetVideoPTS();

		/// Update PTS Offset (if any)
		void UpdatePTSOffset(bool is_video);

		/// Convert Video PTS into Frame Number
		int ConvertVideoPTStoFrame(int pts);

		/// Convert Frame Number into Video PTS
		int ConvertFrameToVideoPTS(int frame_number);

		/// Convert Frame Number into Audio PTS
		int ConvertFrameToAudioPTS(int frame_number);

		/// Calculate Starting video frame and sample # for an audio PTS
		audio_packet_location GetAudioPTSLocation(int pts);

		/// Calculate the # of samples per video frame
		int GetSamplesPerFrame();

		/// Create a new Frame (or return an existing one) and add it to the working queue.
		Frame CreateFrame(int requested_frame);

		/// Check the working queue, and move finished frames to the finished queue
		void CheckWorkingFrames(bool end_of_stream);


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
		void ProcessAudioPacket(int requested_frame, int target_frame, int starting_sample);

		/// Get the next packet (if any)
		int GetNextPacket();

		/// Get an AVFrame (if any)
		bool GetAVFrame();

		/// Check the current seek position and determine if we need to seek again
		bool CheckSeek(bool is_video);

	};

}

#endif
