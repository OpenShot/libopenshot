#ifndef OPENSHOT_IMAGE_READER_H
#define OPENSHOT_IMAGE_READER_H

/**
 * \file
 * \brief Header file for ImageReader class
 * \author Copyright (c) 2011 Jonathan Thomas
 */

#include "FileReaderBase.h"

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
	 * \brief This class uses the ImageMagick++ libraries, to open image files, and return
	 * openshot::Frame objects containing the image.
	 */
	class ImageReader : public FileReaderBase
	{
	private:
		string path;
		Frame* image_frame;
		bool is_open;

	public:

		/// Constructor for ImageReader.  This automatically opens the media file and loads
		/// frame 1, or it throws one of the following exceptions.
		ImageReader(string path);

		/// Close File
		void Close();

		/// Get an openshot::Frame object for a specific frame number of this reader.  All numbers
		/// return the same Frame, since they all share the same image data.
		///
		/// @returns The requested frame (containing the image)
		/// @param[requested_frame] number The frame number that is requested.
		Frame* GetFrame(int requested_frame) throw(ReaderClosed);

		/// Open File - which is called by the constructor automatically
		void Open() throw(InvalidFile);
	};

}

#endif
