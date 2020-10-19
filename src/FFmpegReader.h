/**
 * @file
 * @brief Header file for FFmpegReader class
 * @author Jonathan Thomas <jonathan@openshot.org>, Fabrice Bellard
 *
 * @ref License
 */

/* LICENSE
 *
 * Copyright (c) 2008-2019 OpenShot Studios, LLC, Fabrice Bellard
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
#include "Clip.h"
#include "Exceptions.h"
#include "OpenMPUtilities.h"
#include "Settings.h"


namespace openshot {
	/**
	 * @brief This struct holds the associated video frame and starting sample # for an audio packet.
	 *
	 * Because audio packets do not match up with video frames, this helps determine exactly
	 * where the audio packet's samples belong.
	 */
	struct AudioLocation {
		int64_t frame;
		int sample_start;

		bool is_near(AudioLocation location, int samples_per_frame, int64_t amount);
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
	 * openshot::FFmpegReader r("MyAwesomeVideo.webm");
	 * r.Open(); // Open the reader
	 *
	 * // Get frame number 1 from the video
	 * std::shared_ptr<openshot::Frame> f = r.GetFrame(1);
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
	class FFmpegReader : public ReaderBase {
	private:
		std::string path;

		AVFormatContext *pFormatCtx;
		int i, videoStream, audioStream;
		AVCodecContext *pCodecCtx, *aCodecCtx;
#if HAVE_HW_ACCEL
		AVBufferRef *hw_device_ctx = NULL; //PM
#endif
		AVStream *pStream, *aStream;
		AVPacket *packet;
		AVFrame *pFrame;
		bool is_open;
		bool is_duration_known;
		bool check_interlace;
		bool check_fps;
		bool has_missing_frames;

		CacheMemory working_cache;
		CacheMemory missing_frames;
		std::map<int64_t, int64_t> processing_video_frames;
		std::multimap<int64_t, int64_t> processing_audio_frames;
		std::map<int64_t, int64_t> processed_video_frames;
		std::map<int64_t, int64_t> processed_audio_frames;
		std::multimap<int64_t, int64_t> missing_video_frames;
		std::multimap<int64_t, int64_t> missing_video_frames_source;
		std::multimap<int64_t, int64_t> missing_audio_frames;
		std::multimap<int64_t, int64_t> missing_audio_frames_source;
		std::map<int64_t, int> checked_frames;
		AudioLocation previous_packet_location;

		// DEBUG VARIABLES (FOR AUDIO ISSUES)
		int prev_samples;
		int64_t prev_pts;
		int64_t pts_total;
		int64_t pts_counter;
		int64_t num_packets_since_video_frame;
		int64_t num_checks_since_final;
		std::shared_ptr<openshot::Frame> last_video_frame;

		bool is_seeking;
		int64_t seeking_pts;
		int64_t seeking_frame;
		bool is_video_seek;
		int seek_count;
		int64_t seek_audio_frame_found;
		int64_t seek_video_frame_found;

		int64_t audio_pts_offset;
		int64_t video_pts_offset;
		int64_t last_frame;
		int64_t largest_frame_processed;
		int64_t current_video_frame;    // can't reliably use PTS of video to determine this

		int hw_de_supported = 0;    // Is set by FFmpegReader
#if HAVE_HW_ACCEL
		AVPixelFormat hw_de_av_pix_fmt = AV_PIX_FMT_NONE;
		AVHWDeviceType hw_de_av_device_type = AV_HWDEVICE_TYPE_NONE;
		int IsHardwareDecodeSupported(int codecid);
#endif

		/// Check for the correct frames per second value by scanning the 1st few seconds of video packets.
		void CheckFPS();

		/// Check the current seek position and determine if we need to seek again
		bool CheckSeek(bool is_video);

		/// Check if a frame is missing and attempt to replace its frame image (and
		bool CheckMissingFrame(int64_t requested_frame);

		/// Check the working queue, and move finished frames to the finished queue
		void CheckWorkingFrames(bool end_of_stream, int64_t requested_frame);

		/// Convert Frame Number into Audio PTS
		int64_t ConvertFrameToAudioPTS(int64_t frame_number);

		/// Convert Frame Number into Video PTS
		int64_t ConvertFrameToVideoPTS(int64_t frame_number);

		/// Convert Video PTS into Frame Number
		int64_t ConvertVideoPTStoFrame(int64_t pts);

		/// Create a new Frame (or return an existing one) and add it to the working queue.
		std::shared_ptr<openshot::Frame> CreateFrame(int64_t requested_frame);

		/// Calculate Starting video frame and sample # for an audio PTS
		AudioLocation GetAudioPTSLocation(int64_t pts);

		/// Get an AVFrame (if any)
		bool GetAVFrame();

		/// Get the next packet (if any)
		int GetNextPacket();

		/// Get the smallest video frame that is still being processed
		int64_t GetSmallestVideoFrame();

		/// Get the smallest audio frame that is still being processed
		int64_t GetSmallestAudioFrame();

		/// Get the PTS for the current video packet
		int64_t GetVideoPTS();

		/// Remove partial frames due to seek
		bool IsPartialFrame(int64_t requested_frame);

		/// Process a video packet
		void ProcessVideoPacket(int64_t requested_frame);

		/// Process an audio packet
		void ProcessAudioPacket(int64_t requested_frame, int64_t target_frame, int starting_sample);

		/// Read the stream until we find the requested Frame
		std::shared_ptr<openshot::Frame> ReadStream(int64_t requested_frame);

		/// Remove AVFrame from cache (and deallocate its memory)
		void RemoveAVFrame(AVFrame *);

		/// Remove AVPacket from cache (and deallocate its memory)
		void RemoveAVPacket(AVPacket *);

		/// Seek to a specific Frame.  This is not always frame accurate, it's more of an estimation on many codecs.
		void Seek(int64_t requested_frame);

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

		/// @brief Constructor for FFmpegReader.
		///
		/// Sets (and possibly opens) the media file path,
		/// or throws an exception.
		/// @param path  The filesystem location to load
		/// @param inspect_reader  if true (the default), automatically open the media file and loads frame 1.
		FFmpegReader(const std::string& path, bool inspect_reader=true);

		/// Destructor
		virtual ~FFmpegReader();

		/// Close File
		void Close() override;

		/// Get the cache object used by this reader
		CacheMemory *GetCache() override { return &final_cache; };

		/// Get a shared pointer to a openshot::Frame object for a specific frame number of this reader.
		///
		/// @returns The requested frame of video
		/// @param requested_frame	The frame number that is requested.
		std::shared_ptr<openshot::Frame> GetFrame(int64_t requested_frame) override;

		/// Determine if reader is open or closed
		bool IsOpen() override { return is_open; };

		/// Return the type name of the class
		std::string Name() override { return "FFmpegReader"; };

		/// Get and Set JSON methods
		std::string Json() const override; ///< Generate JSON string of this object
		void SetJson(const std::string value) override; ///< Load JSON string into this object
		Json::Value JsonValue() const override; ///< Generate Json::Value for this object
		void SetJsonValue(const Json::Value root) override; ///< Load Json::Value into this object

		/// Open File - which is called by the constructor automatically
		void Open() override;
	};

}

#endif
