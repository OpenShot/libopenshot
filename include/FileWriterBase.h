#ifndef OPENSHOT_FILE_WRITER_BASE_H
#define OPENSHOT_FILE_WRITER_BASE_H

/**
 * \file
 * \brief Header file for FileWriterBase class
 * \author Copyright (c) 2011 Jonathan Thomas
 */

#include <iostream>
#include <iomanip>
#include "Fraction.h"
#include "Frame.h"

using namespace std;

namespace openshot
{
	/**
	 * \brief This struct contains info about encoding a media file, such as height, width, frames per second, etc...
	 *
	 * Each derived class of FileWriterBase is responsible for updating this struct to reflect accurate information
	 * about the streams.
	 */
	struct WriterInfo
	{
		bool has_video;	///< Determines if this file has a video stream
		bool has_audio;	///< Determines if this file has an audio stream
		float duration;	///< Length of time (in seconds)
		int file_size;	///< Size of file (in bytes)
		int height;		///< The height of the video (in pixels)
		int width;		///< The width of the video (in pixesl)
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
	 * \brief This abstract class is the base class, used by writers.  Writers are types of classes that encode
	 * video, audio, and image files.
	 *
	 * The only requirements for a 'writer', are to derive from this base class, and implement the
	 * WriteFrame method.
	 */
	class FileWriterBase
	{
	public:
		/// Information about the current media file
		WriterInfo info;

		/// This method is required for all derived classes of FileWriterBase.  Add a frame to the stack
		/// waiting to be encoded.
		virtual void AddFrame(Frame* frame) = 0;

		/// This method is required for all derived classes of FileWriterBase.  Write all frames on the
		/// stack.
		virtual void WriteFrames() = 0;

		/// Initialize the values of the WriterInfo struct.  It is important for derived classes to call
		/// this method, or the WriterInfo struct values will not be initialized.
		void InitFileInfo();

		/// Display file information in the standard output stream (stdout)
		void DisplayInfo();
	};

}

#endif
