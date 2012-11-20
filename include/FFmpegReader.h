#ifndef OPENSHOT_FFMPEG_READER_H
#define OPENSHOT_FFMPEG_READER_H

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
#include <tr1/memory>
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
		AVStream *pStream, *aStream;
		AVPacket *packet;
		AVPicture *pFrame;
		bool is_open;

		bool check_interlace;
		bool check_fps;

		int num_of_rescalers;
		int rescaler_position;
		vector<SwsContext*> image_rescalers;
		ReSampleContext *resampleCtx;

		Cache final_cache;
		Cache working_cache;
		map<AVPacket*, AVPacket*> packets;
		map<AVPicture*, AVPicture*> frames;
		map<int, int> processing_video_frames;
		map<int, int> processing_audio_frames;
		audio_packet_location previous_packet_location;

		bool is_seeking;
		int seeking_pts;
		int seeking_frame;
		bool is_video_seek;
		int seek_count;
		int seek_audio_frame_found;
		int seek_video_frame_found;

		int audio_pts_offset;
		int video_pts_offset;
		int last_frame;

		/// Check for the correct frames per second value by scanning the 1st few seconds of video packets.
		void CheckFPS();

		/// Check the current seek position and determine if we need to seek again
		bool CheckSeek(bool is_video);

		/// Check the working queue, and move finished frames to the finished queue
		void CheckWorkingFrames(bool end_of_stream);

		/// Convert image to RGB format
		void convert_image(int current_frame, AVPicture *copyFrame, int width, int height, PixelFormat pix_fmt);

		/// Convert Frame Number into Audio PTS
		int ConvertFrameToAudioPTS(int frame_number);

		/// Convert Frame Number into Video PTS
		int ConvertFrameToVideoPTS(int frame_number);

		/// Convert Video PTS into Frame Number
		int ConvertVideoPTStoFrame(int pts);

		/// Create a new Frame (or return an existing one) and add it to the working queue.
		tr1::shared_ptr<Frame> CreateFrame(int requested_frame);

		/// Calculate Starting video frame and sample # for an audio PTS
		audio_packet_location GetAudioPTSLocation(int pts);

		/// Get an AVFrame (if any)
		bool GetAVFrame();

		/// Get the next packet (if any)
		int GetNextPacket();

		/// Get the smallest video frame that is still being processed
		int GetSmallestVideoFrame();

		/// Get the smallest audio frame that is still being processed
		int GetSmallestAudioFrame();

		/// Calculate the # of samples per video frame (for a specific frame number)
		int GetSamplesPerFrame(int frame_number);

		/// Get the PTS for the current video packet
		int GetVideoPTS();

		/// Init a collection of software rescalers (thread safe)
		void InitScalers();

		/// Process a video packet
		void ProcessVideoPacket(int requested_frame);

		/// Process an audio packet
		void ProcessAudioPacket(int requested_frame, int target_frame, int starting_sample);

		/// Read the stream until we find the requested Frame
		tr1::shared_ptr<Frame> ReadStream(int requested_frame);

		/// Remove AVFrame from cache (and deallocate it's memory)
		void RemoveAVFrame(AVPicture*);

		/// Remove AVPacket from cache (and deallocate it's memory)
		void RemoveAVPacket(AVPacket*);

		/// Remove & deallocate all software scalers
		void RemoveScalers();

		/// Seek to a specific Frame.  This is not always frame accurate, it's more of an estimation on many codecs.
		void Seek(int requested_frame) throw(TooManySeeks);

		/// Update PTS Offset (if any)
		void UpdatePTSOffset(bool is_video);

		/// Update File Info for audio streams
		void UpdateAudioInfo();

		/// Update File Info for video streams
		void UpdateVideoInfo();

	public:

		/// Enable or disable seeking.  Seeking can more quickly locate the requested frame, but some
		/// codecs have trouble seeking, and can introduce artifacts or blank images into the video.
		bool enable_seek;

		/// Constructor for FFmpegReader.  This automatically opens the media file and loads
		/// frame 1, or it throws one of the following exceptions.
		FFmpegReader(string path) throw(InvalidFile, NoStreamsFound, InvalidCodec);

		/// Close File
		void Close();

		/// Get a shared pointer to a openshot::Frame object for a specific frame number of this reader.
		///
		/// @returns The requested frame of video
		/// @param[requested_frame] number The frame number that is requested.
		tr1::shared_ptr<Frame> GetFrame(int requested_frame) throw(ReaderClosed, TooManySeeks);

		/// Get an openshot::Frame object for a specific frame number of this reader.
		///
		/// @returns The requested frame of video
		/// @param[requested_frame] number The frame number that is requested.
		Frame* GetFrameObject(int requested_frame) throw(ReaderClosed, TooManySeeks);

		/// Open File - which is called by the constructor automatically
		void Open() throw(InvalidFile, NoStreamsFound, InvalidCodec);
	};

}

#endif
