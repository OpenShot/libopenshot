#ifndef OPENSHOT_FFMPEG_WRITER_H
#define OPENSHOT_FFMPEG_WRITER_H

/**
 * \file
 * \brief Header file for FFmpegWriter class
 * \author Copyright (c) 2011 Jonathan Thomas
 */

#include "FileReaderBase.h"
#include "FileWriterBase.h"

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
	/**
	 * This enumeration designates which a type of stream when encoding
	 */
	enum Stream_Type
	{
		VIDEO_STREAM,
		AUDIO_STREAM
	};

	/**
	 * \brief This class uses the FFmpeg libraries, to write and encode video files and audio files
	 *
	 * TODO
	 */
	class FFmpegWriter : public FileWriterBase
	{
	private:
		string path;

	public:

		/// Constructor for FFmpegWriter. Throws one of the following exceptions.
		FFmpegWriter(string path) throw(InvalidFile, InvalidFormat, InvalidCodec);

		/// Set video export options
		void SetVideoOptions(string codec, Fraction fps, int width, int height, Fraction display_ratio,
				Fraction pixel_ratio, bool interlaced, bool top_field_first, int bit_rate);

		/// Set audio export options
		void SetAudioOptions(string codec, int sample_rate, int channels, int bit_rate);

		/// Set custom options (some codecs accept additional params)
		void SetOption(Stream_Type stream, string name, double value);

		/// Write the file header (after the options are set)
		void WriteHeader();

		/// Write a single frame
		void WriteFrame(Frame frame);

		/// Write a block of frames from a reader
		void WriteFrame(FileReaderBase* reader, int start, int length);

		/// Write the file trailer (after all frames are written)
		void WriteTrailer();

		/// Close the writer
		void Close();

	};

}

#endif
