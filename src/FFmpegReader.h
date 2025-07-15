/**
 * @file
 * @brief Header file for FFmpegReader class
 * @author Jonathan Thomas <jonathan@openshot.org>, Fabrice Bellard
 *
 * This file is originally based on the Libavformat API example, and then modified
 * by the libopenshot project.
 *
 * @ref License
 */

// Copyright (c) 2008-2019 OpenShot Studios, LLC, Fabrice Bellard
//
// SPDX-License-Identifier: LGPL-3.0-or-later

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
#include "AudioLocation.h"
#include "CacheMemory.h"
#include "Clip.h"
#include "OpenMPUtilities.h"
#include "Settings.h"
#include <cstdlib>


namespace openshot {
	/**
	 * @brief This struct holds the packet counts and end-of-file detection for an openshot::FFmpegReader.
	 *
	 * Keeping track of each packet that is read & decoded helps ensure we process
	 * each packet before EOF is determined. For example, some packets are read async
	 * and decoded later, and tracking the counts makes it easy to continue checking
	 * for a decoded packet until it's ready.
	 */
	struct PacketStatus {
		// Track counts of video and audio packets read & decoded
		int64_t video_read = 0;
		int64_t video_decoded = 0;
		int64_t audio_read = 0;
		int64_t audio_decoded = 0;

		// Track end-of-file detection on video/audio and overall
		bool video_eof = true;
		bool audio_eof = true;
		bool packets_eof = true;
		bool end_of_file = true;

		int64_t packets_read() {
			// Return total packets read
			return video_read + audio_read;
		}

		int64_t packets_decoded() {
			// Return total packets decoded
			return video_decoded + audio_decoded;
		}

		void reset(bool eof) {
			// Reset counts and EOF detection for packets
			video_read = 0; video_decoded = 0; audio_read = 0; audio_decoded = 0;
			video_eof = eof; audio_eof = eof; packets_eof = eof; end_of_file = eof;
		}
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
		int videoStream, audioStream;
		AVCodecContext *pCodecCtx, *aCodecCtx;
#if USE_HW_ACCEL
		AVBufferRef *hw_device_ctx = NULL; //PM
#endif
		AVStream *pStream, *aStream;
		AVPacket *packet;
		AVFrame *pFrame;
		bool is_open;
		bool is_duration_known;
		bool check_interlace;
		bool check_fps;
		int max_concurrent_frames;

		CacheMemory working_cache;
		AudioLocation previous_packet_location;

		// DEBUG VARIABLES (FOR AUDIO ISSUES)
		int prev_samples;
		int64_t prev_pts;
		int64_t pts_total;
		int64_t pts_counter;
		std::shared_ptr<openshot::Frame> last_video_frame;

		bool is_seeking;
		int64_t seeking_pts;
		int64_t seeking_frame;
		bool is_video_seek;
		int seek_count;
		int64_t seek_audio_frame_found;
		int64_t seek_video_frame_found;

		int64_t last_frame;
		int64_t largest_frame_processed;
		int64_t current_video_frame;

		int64_t audio_pts;
		int64_t video_pts;
		bool hold_packet;
		double pts_offset_seconds;
		double audio_pts_seconds;
		double video_pts_seconds;
		int64_t NO_PTS_OFFSET;
		PacketStatus packet_status;

		// Cached conversion contexts and frames for performance
		SwsContext *img_convert_ctx = nullptr;        ///< Cached video scaler context
		SWRCONTEXT *avr_ctx = nullptr;                ///< Cached audio resample context
		AVFrame *pFrameRGB_cached = nullptr;          ///< Temporary frame used for video conversion

		int hw_de_supported = 0;	// Is set by FFmpegReader
#if USE_HW_ACCEL
		AVPixelFormat hw_de_av_pix_fmt = AV_PIX_FMT_NONE;
		AVHWDeviceType hw_de_av_device_type = AV_HWDEVICE_TYPE_NONE;
		int IsHardwareDecodeSupported(int codecid);
#endif

		/// Check for the correct frames per second value by scanning the 1st few seconds of video packets.
		void CheckFPS();

		/// Check the current seek position and determine if we need to seek again
		bool CheckSeek(bool is_video);

		/// Check the working queue, and move finished frames to the finished queue
		void CheckWorkingFrames(int64_t requested_frame);

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

		/// Get the PTS for the current packet
		int64_t GetPacketPTS();

		/// Check if there's an album art
		bool HasAlbumArt();

		/// Remove partial frames due to seek
		bool IsPartialFrame(int64_t requested_frame);

		/// Process a video packet
		void ProcessVideoPacket(int64_t requested_frame);

		/// Process an audio packet
		void ProcessAudioPacket(int64_t requested_frame);

		/// Read the stream until we find the requested Frame
		std::shared_ptr<openshot::Frame> ReadStream(int64_t requested_frame);

		/// Remove AVFrame from cache (and deallocate its memory)
		void RemoveAVFrame(AVFrame *);

		/// Remove AVPacket from cache (and deallocate its memory)
		void RemoveAVPacket(AVPacket *);

		/// Seek to a specific Frame.  This is not always frame accurate, it's more of an estimation on many codecs.
		void Seek(int64_t requested_frame);

		/// Update PTS Offset (presentation time stamp). This shifts timestamps for all streams, so the first timestamp
		/// is always zero. If one stream starts first, it will always be zero, and the other streams shifted
		/// to maintain the correct relative time distance.
		void UpdatePTSOffset();

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

		// Get and Set JSON methods
		std::string Json() const override; ///< Generate JSON string of this object
		void SetJson(const std::string value) override; ///< Load JSON string into this object
		Json::Value JsonValue() const override; ///< Generate Json::Value for this object
		void SetJsonValue(const Json::Value root) override; ///< Load Json::Value into this object

		/// Open File - which is called by the constructor automatically
		void Open() override;

		/// Return true if frame can be read with GetFrame()
		bool GetIsDurationKnown();
	};

}

#endif
