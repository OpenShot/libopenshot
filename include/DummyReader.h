#ifndef OPENSHOT_DUMMY_READER_H
#define OPENSHOT_DUMMY_READER_H

/**
 * \file
 * \brief Header file for ImageReader class
 * \author Copyright (c) 2011 Jonathan Thomas
 */

#include "ReaderBase.h"

#include <cmath>
#include <ctime>
#include <iostream>
#include <omp.h>
#include <stdio.h>
#include <tr1/memory>
#include "Magick++.h"
#include "Cache.h"
#include "Exceptions.h"
#include "FrameRate.h"

using namespace std;

namespace openshot
{
	/**
	 * \brief This class is used as a simple, dummy reader, which always returns a blank frame, and
	 * can be created with any framerate or samplerate.  This is useful in unit tests that need to test
	 * different framerates or samplerates.
	 */
	class DummyReader : public ReaderBase
	{
	private:
		tr1::shared_ptr<Frame> image_frame;
		Framerate fps;
		float duration;
		int sample_rate;
		int width;
		int height;
		int channels;
		bool is_open;

	public:

		/// Constructor for DummyReader.
		DummyReader(Framerate fps, int width, int height, int sample_rate, int channels, float duration);

		/// Close File
		void Close();

		/// Get an openshot::Frame object for a specific frame number of this reader.  All numbers
		/// return the same Frame, since they all share the same image data.
		///
		/// @returns The requested frame (containing the image)
		/// @param[requested_frame] number The frame number that is requested.
		tr1::shared_ptr<Frame> GetFrame(int requested_frame) throw(ReaderClosed);

		/// Open File - which is called by the constructor automatically
		void Open() throw(InvalidFile);
	};

}

#endif
