/**
 * @file
 * @brief Header file for WriterBase class
 * @author Jonathan Thomas <jonathan@openshot.org>
 *
 * @section LICENSE
 *
 * Copyright (c) 2008-2013 OpenShot Studios, LLC
 * (http://www.openshotstudios.com). This file is part of
 * OpenShot Library (http://www.openshot.org), an open-source project
 * dedicated to delivering high quality video editing and animation solutions
 * to the world.
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

#ifndef OPENSHOT_WRITER_BASE_H
#define OPENSHOT_WRITER_BASE_H

#include <iostream>
#include <iomanip>
#include "Fraction.h"
#include "Frame.h"
#include "ReaderBase.h"

using namespace std;

namespace openshot
{
	/**
	 * @brief This struct contains info about encoding a media file, such as height, width, frames per second, etc...
	 *
	 * Each derived class of WriterBase is responsible for updating this struct to reflect accurate information
	 * about the streams.
	 */
	struct WriterInfo
	{
		bool has_video;	///< Determines if this file has a video stream
		bool has_audio;	///< Determines if this file has an audio stream
		float duration;	///< Length of time (in seconds)
		int file_size;	///< Size of file (in bytes)
		int height;		///< The height of the video (in pixels)
		int width;		///< The width of the video (in pixels)
		int pixel_format;	///< The pixel format (i.e. YUV420P, RGB24, etc...)
		Fraction fps;		///< Frames per second, as a fraction (i.e. 24/1 = 24 fps)
		int video_bit_rate;	///< The bit rate of the video stream (in bytes)
		Fraction pixel_ratio;	///< The pixel ratio of the video stream as a fraction (i.e. some pixels are not square)
		Fraction display_ratio;	///< The ratio of width to height of the video stream (i.e. 640x480 has a ratio of 4/3)
		string vcodec;		///< The name of the video codec used to encode / decode the video stream
		long int video_length;	///< The number of frames in the video stream
		int video_stream_index;		///< The index of the video stream
		Fraction video_timebase;	///< The video timebase determines how long each frame stays on the screen
		bool interlaced_frame;	// Are the contents of this frame interlaced
		bool top_field_first;	// Which interlaced field should be displayed first
		string acodec;		///< The name of the audio codec used to encode / decode the video stream
		int audio_bit_rate;	///< The bit rate of the audio stream (in bytes)
		int sample_rate;	///< The number of audio samples per second (44100 is a common sample rate)
		int channels;		///< The number of audio channels used in the audio stream
		int audio_stream_index;		///< The index of the audio stream
		Fraction audio_timebase;	///< The audio timebase determines how long each audio packet should be played
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
		/// Information about the current media file
		WriterInfo info;

		/// This method copy's the info struct of a reader, and sets the writer with the same info
		void CopyReaderInfo(ReaderBase* reader);

		/// This method is required for all derived classes of WriterBase.  Write a Frame to the video file.
		virtual void WriteFrame(tr1::shared_ptr<Frame> frame) = 0;

		/// This method is required for all derived classes of WriterBase.  Write a block of frames from a reader.
		virtual void WriteFrame(ReaderBase* reader, int start, int length) = 0;

		/// Initialize the values of the WriterInfo struct.  It is important for derived classes to call
		/// this method, or the WriterInfo struct values will not be initialized.
		void InitFileInfo();

		/// Get and Set JSON methods
		string Json(); ///< Generate JSON string of this object
		Json::Value JsonValue(); ///< Generate Json::JsonValue for this object
		void SetJson(string value) throw(InvalidJSON); ///< Load JSON string into this object
		void SetJsonValue(Json::Value root); ///< Load Json::JsonValue into this object

		/// Display file information in the standard output stream (stdout)
		void DisplayInfo();
	};

}

#endif
