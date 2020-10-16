/**
 * @file
 * @brief Header file for TextReader class
 * @author Jonathan Thomas <jonathan@openshot.org>
 *
 * @ref License
 */

/* LICENSE
 *
 * Copyright (c) 2008-2019 OpenShot Studios, LLC
 * <http://www.openshotstudios.com/>. This file is part of
 * OpenShot Library (libopenshot), an open-source project dedicated to
 * delivering high quality video editing and animation solutions to the
 * world. For more information visit <http://www.openshot.org/>.
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

#ifndef OPENSHOT_TEXT_READER_H
#define OPENSHOT_TEXT_READER_H

// Require ImageMagick support
#ifdef USE_IMAGEMAGICK

#include "ReaderBase.h"

#include <cmath>
#include <ctime>
#include <iostream>
#include <omp.h>
#include <stdio.h>
#include <memory>
#include "CacheMemory.h"
#include "Enums.h"
#include "Exceptions.h"
#include "MagickUtilities.h"

namespace openshot
{

	/**
	 * @brief This class uses the ImageMagick++ libraries, to create frames with "Text", and return
	 * openshot::Frame objects.
	 *
	 * All system fonts are supported, including many different font properties, such as size, color,
	 * alignment, padding, etc...
	 *
	 * @code
	 * // Create a reader to generate an openshot::Frame containing text
	 * TextReader r(720, // width
	 *              480, // height
	 *              5, // x_offset
	 *              5, // y_offset
	 *              GRAVITY_CENTER, // gravity
	 *              "Check out this Text!", // text
	 *              "Arial", // font
	 *              15.0, // size
	 *              "#fff000", // text_color
	 *              "#000000" // background_color
	 *              );
	 * r.Open(); // Open the reader
	 *
	 * // Get frame number 1 from the video (in fact, any frame # you request will return the same frame)
	 * std::shared_ptr<Frame> f = r.GetFrame(1);
	 *
	 * // Now that we have an openshot::Frame object, lets have some fun!
	 * f->Display(); // Display the frame on the screen
	 *
	 * // Close the reader
	 * r.Close();
	 * @endcode
	 */
	class TextReader : public ReaderBase
	{
	private:
		int width;
		int height;
		int x_offset;
		int y_offset;
		std::string text;
		std::string font;
		double size;
		std::string text_color;
		std::string background_color;
		std::string text_background_color;
		std::shared_ptr<Magick::Image> image;
		MAGICK_DRAWABLE lines;
		bool is_open;
		openshot::GravityType gravity;

	public:

		/// Default constructor (blank text)
		TextReader();

		/// @brief Constructor for TextReader with all parameters.
		/// @param width The width of the requested openshot::Frame (not the size of the text)
		/// @param height The height of the requested openshot::Frame (not the size of the text)
		/// @param x_offset The number of pixels to offset the text on the X axis (horizontal)
		/// @param y_offset The number of pixels to offset the text on the Y axis (vertical)
		/// @param gravity The alignment / gravity of the text
		/// @param text The text you want to generate / display
		/// @param font The font of the text
		/// @param size The size of the text
		/// @param text_color The color of the text
		/// @param background_color The background color of the text frame image (also supports Transparent)
		TextReader(int width, int height, int x_offset, int y_offset, GravityType gravity, std::string text, std::string font, double size, std::string text_color, std::string background_color);

		/// Draw a box under rendered text using the specified color.
		/// @param color The background color behind the text
		void SetTextBackgroundColor(std::string color);

		/// Close Reader
		void Close() override;

		/// Get the cache object used by this reader (always returns NULL for this object)
		openshot::CacheMemory* GetCache() override { return NULL; };

		/// Get an openshot::Frame object for a specific frame number of this reader.  All numbers
		/// return the same Frame, since they all share the same image data.
		///
		/// @returns The requested frame (containing the image)
		/// @param requested_frame The frame number that is requested.
		std::shared_ptr<openshot::Frame> GetFrame(int64_t requested_frame) override;

		/// Determine if reader is open or closed
		bool IsOpen() override { return is_open; };

		/// Return the type name of the class
		std::string Name() override { return "TextReader"; };

		/// Get and Set JSON methods
		std::string Json() const override; ///< Generate JSON string of this object
		void SetJson(const std::string value) override; ///< Load JSON string into this object
		Json::Value JsonValue() const override; ///< Generate Json::Value for this object
		void SetJsonValue(const Json::Value root) override; ///< Load Json::Value into this object

		/// Open Reader - which is called by the constructor automatically
		void Open() override;
	};

}

#endif //USE_IMAGEMAGICK
#endif //OPENSHOT_TEXT_READER_H
