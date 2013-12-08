/**
 * @file
 * @brief Header file for FFmpegReader class
 * @author Jonathan Thomas <jonathan@openshot.org>, Fabrice Bellard
 *
 * @section LICENSE
 *
 * Copyright (c) 2008-2013 OpenShot Studios, LLC, Fabrice Bellard
 * (http://www.openshotstudios.com). This file is part of
 * OpenShot Library (http://www.openshot.org), an open-source project
 * dedicated to delivering high quality video editing and animation solutions
 * to the world.
 *
 * This file is originally based on the Libavformat API example, and then modified
 * by the libopenshot project.
 *
 * OpenShot Library is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * OpenShot Library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with OpenShot Library.  If not, see <http://www.gnu.org/licenses/>.
 */

#ifndef OPENSHOT_FFMPEG_READER_H
#define OPENSHOT_FFMPEG_READER_H

#include "ReaderBase.h"

// Include FFmpeg headers and macros
#include "FFmpegUtilities.h"

#include <cmath>
#include <ctime>
#include <iostream>
#include <omp.h>
#include <stdio.h>
#include <tr1/memory>
#include "Magick++.h"
#include "Cache.h"
#include "Exceptions.h"
#include "Sleep.h"


using namespace std;

namespace openshot
{
	/**
	 * @brief This struct holds the associated video frame and starting sample # for an audio packet.
	 *
	 * Because audio packets do not match up with video frames, this helps determine exactly
	 * where the audio packet's samples belong.
	 */
	struct AudioLocation
	{
		int frame;
		int sample_start;
		int is_near(AudioLocation location, int samples_per_frame, int amount);
	};

	/**
	 * @brief This class uses the FFmpeg libraries, to open video files and audio files, and return
	 * openshot::Frame objects for any frame in the file.
	 *
	 * All seeking and caching is handled internally, and the primary public interface is the GetFrame()
	 * method.  To use this reader, simply create an instance of this class, and call the GetFrame method
	 * to start retrieving frames.  Use the <b>info</b> struct to obtain information on the file, such as the length
	 * (# of frames), height, width, bit rate, frames per second (fps), etc...
	 *
	 * @code
	 * // Create a reader for a video
	 * FFmpegReader r("MyAwesomeVideo.webm");
	 * r.Open(); // Open the reader
	 *
	 * // Get frame number 1 from the video
	 * tr1::shared_ptr<Frame> f = r.GetFrame(1);
	 *
	 * // Now that we have an openshot::Frame object, lets have some fun!
	 * f->Display(); // Display the frame on the screen
	 * f->DisplayWaveform(); // Display the audio waveform as an image
	 * f->Play(); // Play the audio through your speaker
	 *
	 * // Close the reader
	 * r.Close();
	 * @endcode
	 */
	class FFmpegReader : public ReaderBase
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
		bool is_duration_known;

		bool check_interlace;
		bool check_fps;

		int num_of_rescalers;
		int rescaler_position;
		vector<SwsContext*> image_rescalers;
		ReSampleContext *resampleCtx;

		Cache working_cache;
		map<AVPacket*, AVPacket*> packets;
		map<AVPicture*, AVPicture*> frames;
		map<int, int> processing_video_frames;
		map<int, int> processing_audio_frames;
		AudioLocation previous_packet_location;

		// DEBUG VARIABLES (FOR AUDIO ISSUES)
		bool display_debug;
		int prev_samples;
		int prev_pts;
		int pts_total;
		int pts_counter;

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
		AudioLocation GetAudioPTSLocation(int pts);

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
		/// Final cache object used to hold final frames
		Cache final_cache;

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
		/// @param requested_frame	The frame number that is requested.
		tr1::shared_ptr<Frame> GetFrame(int requested_frame) throw(ReaderClosed, TooManySeeks);

		/// Get and Set JSON methods
		string Json(); ///< Generate JSON string of this object
		void SetJson(string value) throw(InvalidJSON); ///< Load JSON string into this object
		Json::Value JsonValue(); ///< Generate Json::JsonValue for this object
		void SetJsonValue(Json::Value root) throw(InvalidFile); ///< Load Json::JsonValue into this object

		/// Open File - which is called by the constructor automatically
		void Open() throw(InvalidFile, NoStreamsFound, InvalidCodec);
	};

}

#endif
