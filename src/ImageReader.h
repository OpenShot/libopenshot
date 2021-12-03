/**
 * @file
 * @brief Header file for ImageReader class
 * @author Jonathan Thomas <jonathan@openshot.org>
 *
 * @ref License
 */

// Copyright (c) 2008-2019 OpenShot Studios, LLC
//
// SPDX-License-Identifier: LGPL-3.0-or-later

#ifndef OPENSHOT_IMAGE_READER_H
#define OPENSHOT_IMAGE_READER_H

// Require ImageMagick support
#ifdef USE_IMAGEMAGICK

#include <memory>
#include <string>

#include "ReaderBase.h"
#include "Json.h"

// Forward decls
namespace Magick {
    class Image;
}
namespace openshot {
	class CacheBase;
	class Frame;
}

namespace openshot
{
	/**
	 * @brief This class uses the ImageMagick++ libraries, to open image files, and return
	 * openshot::Frame objects containing the image.
	 *
	 * @code
	 * // Create a reader for a video
	 * ImageReader r("MyAwesomeImage.jpeg");
	 * r.Open(); // Open the reader
	 *
	 * // Get frame number 1 from the video
	 * std::shared_ptr<Frame> f = r.GetFrame(1);
	 *
	 * // Now that we have an openshot::Frame object, lets have some fun!
	 * f->Display(); // Display the frame on the screen
	 *
	 * // Close the reader
	 * r.Close();
	 * @endcode
	 */
	class ImageReader : public ReaderBase
	{
	private:
		std::string path;
		std::shared_ptr<Magick::Image> image;
		bool is_open;

	public:
		/// @brief Constructor for ImageReader.
		///
		/// Opens the media file to inspect its properties and loads frame 1,
		/// iff inspect_reader == true (the default). Pass a false value in
		/// the optional parameter to defer this initial Open()/Close() cycle.
		///
		/// When not inspecting the media file, it's much faster, and useful
		/// when you are inflating the object using JSON after instantiation.
		ImageReader(const std::string& path, bool inspect_reader=true);

		/// Close File
		void Close() override;

		/// Get the cache object used by this reader (always returns NULL for this object)
		CacheBase* GetCache() override { return NULL; };

		/// Get an openshot::Frame object for a specific frame number of this reader.  All numbers
		/// return the same Frame, since they all share the same image data.
		///
		/// @returns The requested frame (containing the image)
		/// @param requested_frame The frame number that is requested.
		std::shared_ptr<Frame> GetFrame(int64_t requested_frame) override;

		/// Determine if reader is open or closed
		bool IsOpen() override { return is_open; };

		/// Return the type name of the class
		std::string Name() override { return "ImageReader"; };

		// Get and Set JSON methods
		std::string Json() const override; ///< Generate JSON string of this object
		void SetJson(const std::string value) override; ///< Load JSON string into this object
		Json::Value JsonValue() const override; ///< Generate Json::Value for this object
		void SetJsonValue(const Json::Value root) override; ///< Load Json::Value into this object

		/// Open File - which is called by the constructor automatically
		void Open() override;
	};

}

#endif //USE_IMAGEMAGICK
#endif //OPENSHOT_IMAGE_READER_H
