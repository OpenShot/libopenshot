/**
 * @file
 * @brief Header file for ReaderBase class
 * @author Jonathan Thomas <jonathan@openshot.org>
 *
 * @ref License
 */

// Copyright (c) 2008-2019 OpenShot Studios, LLC
//
// SPDX-License-Identifier: LGPL-3.0-or-later

#ifndef OPENSHOT_READER_BASE_H
#define OPENSHOT_READER_BASE_H

#include <map>
#include <memory>
#include <mutex>
#include <iostream>
#include <string>
#include <vector>

#include "ChannelLayouts.h"
#include "Fraction.h"
#include "Json.h"

namespace openshot
{
	class CacheBase;
	class ClipBase;
	class Frame;
	/**
	 * @brief This struct contains info about a media file, such as height, width, frames per second, etc...
	 *
	 * Each derived class of ReaderBase is responsible for updating this struct to reflect accurate information
	 * about the streams.
	 */
	struct ReaderInfo
	{
		bool has_video;				///< Determines if this file has a video stream
		bool has_audio;				///< Determines if this file has an audio stream
		bool has_single_image;		///< Determines if this file only contains a single image
		float duration;				///< Length of time (in seconds)
		int64_t file_size;		///< Size of file (in bytes)
		int height;					///< The height of the video (in pixels)
		int width;					///< The width of the video (in pixesl)
		int pixel_format;			///< The pixel format (i.e. YUV420P, RGB24, etc...)
		openshot::Fraction fps;				///< Frames per second, as a fraction (i.e. 24/1 = 24 fps)
		int video_bit_rate;			///< The bit rate of the video stream (in bytes)
		openshot::Fraction pixel_ratio;		///< The pixel ratio of the video stream as a fraction (i.e. some pixels are not square)
		openshot::Fraction display_ratio;		///< The ratio of width to height of the video stream (i.e. 640x480 has a ratio of 4/3)
		std::string vcodec;				///< The name of the video codec used to encode / decode the video stream
		int64_t video_length;		///< The number of frames in the video stream
		int video_stream_index;		///< The index of the video stream
		openshot::Fraction video_timebase;	///< The video timebase determines how long each frame stays on the screen
		bool interlaced_frame;		// Are the contents of this frame interlaced
		bool top_field_first;		// Which interlaced field should be displayed first
		std::string acodec;				///< The name of the audio codec used to encode / decode the video stream
		int audio_bit_rate;			///< The bit rate of the audio stream (in bytes)
		int sample_rate;			///< The number of audio samples per second (44100 is a common sample rate)
		int channels;				///< The number of audio channels used in the audio stream
		openshot::ChannelLayout channel_layout;	///< The channel layout (mono, stereo, 5 point surround, etc...)
		int audio_stream_index;		///< The index of the audio stream
		openshot::Fraction audio_timebase;	///< The audio timebase determines how long each audio packet should be played
		std::map<std::string, std::string> metadata;	///< An optional map/dictionary of metadata for this reader
	};

	/**
	 * @brief This abstract class is the base class, used by all readers in libopenshot.
	 *
	 * Readers are types of classes that read video, audio, and image files, and
	 * return openshot::Frame objects. The only requirements for a 'reader', are to
	 * derive from this base class, implement the GetFrame method, and populate ReaderInfo.
	 */
	class ReaderBase
	{
	protected:
		/// Mutex for multiple threads
		std::recursive_mutex getFrameMutex;
		openshot::ClipBase* clip; ///< Pointer to the parent clip instance (if any)

	public:

		/// Constructor for the base reader, where many things are initialized.
	    ReaderBase();

		/// Information about the current media file
		openshot::ReaderInfo info;

		/// Parent clip object of this reader (which can be unparented and NULL)
		openshot::ClipBase* ParentClip();

		/// Set parent clip object of this reader
		void ParentClip(openshot::ClipBase* new_clip);

		/// Close the reader (and any resources it was consuming)
		virtual void Close() = 0;

		/// Display file information in the standard output stream (stdout)
		void DisplayInfo(std::ostream* out=&std::cout);

		/// Get the cache object used by this reader (note: not all readers use cache)
		virtual openshot::CacheBase* GetCache() = 0;

		/// This method is required for all derived classes of ReaderBase, and returns the
		/// openshot::Frame object, which contains the image and audio information for that
		/// frame of video.
		///
		/// @returns The requested frame of video
		/// @param[in] number The frame number that is requested.
		virtual std::shared_ptr<openshot::Frame> GetFrame(int64_t number) = 0;

		/// Determine if reader is open or closed
		virtual bool IsOpen() = 0;

		/// Return the type name of the class
		virtual std::string Name() = 0;

		// Get and Set JSON methods
		virtual std::string Json() const = 0; ///< Generate JSON string of this object
		virtual void SetJson(const std::string value) = 0; ///< Load JSON string into this object
		virtual Json::Value JsonValue() const = 0; ///< Generate Json::Value for this object
		virtual void SetJsonValue(const Json::Value root) = 0; ///< Load Json::Value into this object

		/// Open the reader (and start consuming resources, such as images or video files)
		virtual void Open() = 0;

		virtual ~ReaderBase() = default;
	};

}

#endif
