/**
 * @file
 * @brief Header file for QtTextReader class
 * @author Jonathan Thomas <jonathan@openshot.org>
 * @author Sergei Kolesov (jediserg)
 * @author Jeff Shillitto (jeffski)
 *
 * @ref License
 */

// Copyright (c) 2008-2019 OpenShot Studios, LLC
//
// SPDX-License-Identifier: LGPL-3.0-or-later

#ifndef OPENSHOT_QT_TEXT_READER_H
#define OPENSHOT_QT_TEXT_READER_H

#include "ReaderBase.h"

#include <memory>

#include "Enums.h"

#include <QFont>

class QImage;

namespace openshot
{
	// Forward decls
	class CacheBase;
	class Frame;

	/**
	 * @brief This class uses Qt libraries, to create frames with "Text", and return
	 * openshot::Frame objects.
	 *
	 * All system fonts are supported, including many different font properties, such as size, color,
	 * alignment, padding, etc...
	 *
	 * @code
	 * // Any application using this class must instantiate either QGuiApplication or QApplication
	 * QApplication a(argc, argv);
	 *
	 * // Create a reader to generate an openshot::Frame containing text
	 * QtTextReader r(720, // width
	 *              480, // height
	 *              5, // x_offset
	 *              5, // y_offset
	 *              GRAVITY_CENTER, // gravity
	 *              "Check out this Text!", // text
	 *              "Arial", // font
	 *              15.0, // font size
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
	class QtTextReader : public ReaderBase
	{
	private:
		int width;
		int height;
		int x_offset;
		int y_offset;
		std::string text;
		QFont font;
		std::string text_color;
		std::string background_color;
		std::string text_background_color;
		std::shared_ptr<QImage> image;
		bool is_open;
		openshot::GravityType gravity;

	public:

		/// Default constructor (blank text)
		QtTextReader();

		/// @brief Constructor for QtTextReader with all parameters.
		/// @param width The width of the requested openshot::Frame (not the size of the text)
		/// @param height The height of the requested openshot::Frame (not the size of the text)
		/// @param x_offset The number of pixels to offset the text on the X axis (horizontal)
		/// @param y_offset The number of pixels to offset the text on the Y axis (vertical)
		/// @param gravity The alignment / gravity of the text
		/// @param text The text you want to generate / display
		/// @param font The font of the text
		/// @param text_color The color of the text (valid values are a color string in \#RRGGBB or \#AARRGGBB notation or a CSS color name)
		/// @param background_color The background color of the frame image (valid values are a color string in \#RRGGBB or \#AARRGGBB notation, a CSS color name, or 'transparent')
		QtTextReader(int width, int height, int x_offset, int y_offset, GravityType gravity, std::string text, QFont font, std::string text_color, std::string background_color);

		/// Draw a box under rendered text using the specified color.
		/// @param color The background color behind the text (valid values are a color string in \#RRGGBB or \#AARRGGBB notation or a CSS color name)
		void SetTextBackgroundColor(std::string color);

		/// Close Reader
		void Close() override;

		/// Get the cache object used by this reader (always returns NULL for this object)
		CacheBase* GetCache() override { return NULL; };

		/// Get an openshot::Frame object for a specific frame number of this reader.  All numbers
		/// return the same Frame, since they all share the same image data.
		///
		/// @returns The requested frame (containing the image)
		/// @param requested_frame The frame number that is requested.
		std::shared_ptr<openshot::Frame> GetFrame(int64_t requested_frame) override;

		/// Determine if reader is open or closed
		bool IsOpen() override { return is_open; };

		/// Return the type name of the class
		std::string Name() override { return "QtTextReader"; };

		// Get and Set JSON methods
		std::string Json() const override; ///< Generate JSON string of this object
		void SetJson(const std::string value) override; ///< Load JSON string into this object
		Json::Value JsonValue() const override; ///< Generate Json::Value for this object
		void SetJsonValue(const Json::Value root) override; ///< Load Json::Value into this object

		/// Open Reader - which is called by the constructor automatically
		void Open() override;
	};

}

#endif
