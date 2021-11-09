/**
 * @file
 * @brief Header file for WriterBase class
 * @author Jonathan Thomas <jonathan@openshot.org>
 *
 * @ref License
 */

// Copyright (c) 2008-2019 OpenShot Studios, LLC
//
// SPDX-License-Identifier: LGPL-3.0-or-later

#ifndef OPENSHOT_WRITER_BASE_H
#define OPENSHOT_WRITER_BASE_H

#include <iostream>

#include "ChannelLayouts.h"
#include "Fraction.h"
#include "Json.h"

namespace openshot
{
	class ReaderBase;
	class Frame;
	/**
	 * @brief This struct contains info about encoding a media file, such as height, width, frames per second, etc...
	 *
	 * Each derived class of WriterBase is responsible for updating this struct to reflect accurate information
	 * about the streams.
	 */
	struct WriterInfo
	{
		bool has_video;				///< Determines if this file has a video stream
		bool has_audio;				///< Determines if this file has an audio stream
		bool has_single_image;		///< Determines if this file only contains a single image
		float duration;				///< Length of time (in seconds)
		int64_t file_size;				///< Size of file (in bytes)
		int height;					///< The height of the video (in pixels)
		int width;					///< The width of the video (in pixels)
		int pixel_format;			///< The pixel format (i.e. YUV420P, RGB24, etc...)
		openshot::Fraction fps;				///< Frames per second, as a fraction (i.e. 24/1 = 24 fps)
		int video_bit_rate;		///< The bit rate of the video stream (in bytes)
		openshot::Fraction pixel_ratio;		///< The pixel ratio of the video stream as a fraction (i.e. some pixels are not square)
		openshot::Fraction display_ratio;		///< The ratio of width to height of the video stream (i.e. 640x480 has a ratio of 4/3)
		std::string vcodec;				///< The name of the video codec used to encode / decode the video stream
		int64_t video_length;		///< The number of frames in the video stream
		int video_stream_index;		///< The index of the video stream
		openshot::Fraction video_timebase;	///< The video timebase determines how long each frame stays on the screen
		bool interlaced_frame;		///< Are the contents of this frame interlaced
		bool top_field_first;		///< Which interlaced field should be displayed first
		std::string acodec;				///< The name of the audio codec used to encode / decode the video stream
		int audio_bit_rate;		///< The bit rate of the audio stream (in bytes)
		int sample_rate;			///< The number of audio samples per second (44100 is a common sample rate)
		int channels;				///< The number of audio channels used in the audio stream
		openshot::ChannelLayout channel_layout;	///< The channel layout (mono, stereo, 5 point surround, etc...)
		int audio_stream_index;		///< The index of the audio stream
		openshot::Fraction audio_timebase;	///< The audio timebase determines how long each audio packet should be played
		std::map<std::string, std::string> metadata;	///< An optional map/dictionary of video & audio metadata
	};

	/**
	 * @brief This abstract class is the base class, used by writers.  Writers are types of classes that encode
	 * video, audio, and image files.
	 *
	 * The only requirements for a 'writer', are to derive from this base class, and implement the
	 * WriteFrame method.
	 */
	class WriterBase
	{
	public:
		/// Constructor for WriterBase class, many things are initialized here
		WriterBase();

		/// Information about the current media file
		WriterInfo info;

		/// @brief This method copy's the info struct of a reader, and sets the writer with the same info
		/// @param reader The source reader to copy
		void CopyReaderInfo(openshot::ReaderBase* reader);

		/// Determine if writer is open or closed
		virtual bool IsOpen() = 0;

		/// This method is required for all derived classes of WriterBase.  Write a Frame to the video file.
		virtual void WriteFrame(std::shared_ptr<openshot::Frame> frame) = 0;

		/// This method is required for all derived classes of WriterBase.  Write a block of frames from a reader.
		virtual void WriteFrame(openshot::ReaderBase* reader, int64_t start, int64_t length) = 0;

		// Get and Set JSON methods
		std::string Json() const; ///< Generate JSON string of this object
		Json::Value JsonValue() const; ///< Generate Json::Value for this object
		void SetJson(const std::string value); ///< Load JSON string into this object
		void SetJsonValue(const Json::Value root); ///< Load Json::Value into this object

		/// Display file information in the standard output stream (stdout)
		void DisplayInfo(std::ostream* out=&std::cout);

		/// Open the writer (and start initializing streams)
		virtual void Open() = 0;

		virtual ~WriterBase() = default;
	};

}

#endif
