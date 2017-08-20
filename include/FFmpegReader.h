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
 * OpenShot Library (libopenshot) is free software: you can redistribute it
 * and/or modify it under the terms of the GNU Lesser General Public License
 * as published by the Free Software Foundation, either version 3 of the
 * License, or (at your option) any later version.
 *
 * OpenShot Library (libopenshot) is distributed in the hope that it will be
 * useful, but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public License
 * along with OpenShot Library. If not, see <http://www.gnu.org/licenses/>.
 */

#ifndef OPENSHOT_FFMPEG_READER_H
#define OPENSHOT_FFMPEG_READER_H

#include "ReaderBase.h"

// Include FFmpeg headers and macros
#include "FFmpegUtilities.h"

#include <cmath>
#include <ctime>
#include <iostream>
#include <stdio.h>
#include <memory>
#include "CacheMemory.h"
#include "Exceptions.h"
#include "OpenMPUtilities.h"


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
		long int frame;
		int sample_start;
		bool is_near(AudioLocation location, int samples_per_frame, long int amount);
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
	 * std::shared_ptr<Frame> f = r.GetFrame(1);
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
		bool has_missing_frames;

		CacheMemory working_cache;
		CacheMemory missing_frames;
		map<long int, long int> processing_video_frames;
		multimap<long int, long int> processing_audio_frames;
		map<long int, long int> processed_video_frames;
		map<long int, long int> processed_audio_frames;
		multimap<long int, long int> missing_video_frames;
		multimap<long int, long int> missing_video_frames_source;
		multimap<long int, long int> missing_audio_frames;
		multimap<long int, long int> missing_audio_frames_source;
		map<long int, int> checked_frames;
		AudioLocation previous_packet_location;

		// DEBUG VARIABLES (FOR AUDIO ISSUES)
		int prev_samples;
		long int prev_pts;
		long int pts_total;
		long int pts_counter;
		long int num_packets_since_video_frame;
		long int num_checks_since_final;
		std::shared_ptr<Frame> last_video_frame;

		bool is_seeking;
		long int seeking_pts;
		long int seeking_frame;
		bool is_video_seek;
		int seek_count;
		long int seek_audio_frame_found;
		long int seek_video_frame_found;

		long int audio_pts_offset;
		long int video_pts_offset;
		long int last_frame;
		long int largest_frame_processed;
		long int current_video_frame;	// can't reliably use PTS of video to determine this

		/// Check for the correct frames per second value by scanning the 1st few seconds of video packets.
		void CheckFPS();

		/// Check the current seek position and determine if we need to seek again
		bool CheckSeek(bool is_video);

		/// Check if a frame is missing and attempt to replace it's frame image (and
		bool CheckMissingFrame(long int requested_frame);

		/// Check the working queue, and move finished frames to the finished queue
		void CheckWorkingFrames(bool end_of_stream, long int requested_frame);

		/// Convert image to RGB format
		void convert_image(long int current_frame, AVPicture *copyFrame, int width, int height, PixelFormat pix_fmt);

		/// Convert Frame Number into Audio PTS
		long int ConvertFrameToAudioPTS(long int frame_number);

		/// Convert Frame Number into Video PTS
		long int ConvertFrameToVideoPTS(long int frame_number);

		/// Convert Video PTS into Frame Number
		long int ConvertVideoPTStoFrame(long int pts);

		/// Create a new Frame (or return an existing one) and add it to the working queue.
		std::shared_ptr<Frame> CreateFrame(long int requested_frame);

		/// Calculate Starting video frame and sample # for an audio PTS
		AudioLocation GetAudioPTSLocation(long int pts);

		/// Get an AVFrame (if any)
		bool GetAVFrame();

		/// Get the next packet (if any)
		int GetNextPacket();

		/// Get the smallest video frame that is still being processed
		long int GetSmallestVideoFrame();

		/// Get the smallest audio frame that is still being processed
		long int GetSmallestAudioFrame();

		/// Get the PTS for the current video packet
		long int GetVideoPTS();

		/// Remove partial frames due to seek
		bool IsPartialFrame(long int requested_frame);

		/// Process a video packet
		void ProcessVideoPacket(long int requested_frame);

		/// Process an audio packet
		void ProcessAudioPacket(long int requested_frame, long int target_frame, int starting_sample);

		/// Read the stream until we find the requested Frame
		std::shared_ptr<Frame> ReadStream(long int requested_frame);

		/// Remove AVFrame from cache (and deallocate it's memory)
		void RemoveAVFrame(AVPicture*);

		/// Remove AVPacket from cache (and deallocate it's memory)
		void RemoveAVPacket(AVPacket*);

		/// Seek to a specific Frame.  This is not always frame accurate, it's more of an estimation on many codecs.
		void Seek(long int requested_frame) throw(TooManySeeks);

		/// Update PTS Offset (if any)
		void UpdatePTSOffset(bool is_video);

		/// Update File Info for audio streams
		void UpdateAudioInfo();

		/// Update File Info for video streams
		void UpdateVideoInfo();

	public:
		/// Final cache object used to hold final frames
		CacheMemory final_cache;

		/// Enable or disable seeking.  Seeking can more quickly locate the requested frame, but some
		/// codecs have trouble seeking, and can introduce artifacts or blank images into the video.
		bool enable_seek;

		/// Constructor for FFmpegReader.  This automatically opens the media file and loads
		/// frame 1, or it throws one of the following exceptions.
		FFmpegReader(string path) throw(InvalidFile, NoStreamsFound, InvalidCodec);

		/// Constructor for FFmpegReader.  This only opens the media file to inspect it's properties
		/// if inspect_reader=true. When not inspecting the media file, it's much faster, and useful
		/// when you are inflating the object using JSON after instantiating it.
		FFmpegReader(string path, bool inspect_reader) throw(InvalidFile, NoStreamsFound, InvalidCodec);

		/// Destructor
		~FFmpegReader();

		/// Close File
		void Close();

		/// Get the cache object used by this reader
		CacheMemory* GetCache() { return &final_cache; };

		/// Get a shared pointer to a openshot::Frame object for a specific frame number of this reader.
		///
		/// @returns The requested frame of video
		/// @param requested_frame	The frame number that is requested.
		std::shared_ptr<Frame> GetFrame(long int requested_frame) throw(OutOfBoundsFrame, ReaderClosed, TooManySeeks);

		/// Determine if reader is open or closed
		bool IsOpen() { return is_open; };

		/// Return the type name of the class
		string Name() { return "FFmpegReader"; };

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
