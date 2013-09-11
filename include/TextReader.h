#ifndef OPENSHOT_TEXT_READER_H
#define OPENSHOT_TEXT_READER_H

/**
 * @file
 * @brief Header file for TextReader class
 * @author Copyright (c) 2008-2013 OpenShot Studios, LLC
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
#include "Clip.h"
#include "Exceptions.h"

using namespace std;

namespace openshot
{

	/**
	 * @brief This class uses the ImageMagick++ libraries, to create frames with "Text", and return
	 * openshot::Frame objects.
	 *
	 * All system fonts are supported, including many different font properties, such as size, color,
	 * alignment, padding, etc...
	 */
	class TextReader : public ReaderBase
	{
	private:
		int width;
		int height;
		int x_offset;
		int y_offset;
		string text;
		string font;
		double size;
		string text_color;
		string background_color;
		tr1::shared_ptr<Magick::Image> image;
		list<Magick::Drawable> lines;
		bool is_open;
		GravityType gravity;

	public:

		/// Constructor for TextReader.
		TextReader(int width, int height, int x_offset, int y_offset, GravityType gravity, string text, string font, double size, string text_color, string background_color);

		/// Close Reader
		void Close();

		/// Get an openshot::Frame object for a specific frame number of this reader.  All numbers
		/// return the same Frame, since they all share the same image data.
		///
		/// @returns The requested frame (containing the image)
		/// @param[requested_frame] number The frame number that is requested.
		tr1::shared_ptr<Frame> GetFrame(int requested_frame) throw(ReaderClosed);

		/// Open Reader - which is called by the constructor automatically
		void Open();
	};

}

#endif
