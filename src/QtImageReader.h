/**
 * @file
 * @brief Header file for QtImageReader class
 * @author Jonathan Thomas <jonathan@openshot.org>
 *
 * @ref License
 */

// Copyright (c) 2008-2019 OpenShot Studios, LLC
//
// SPDX-License-Identifier: LGPL-3.0-or-later

#ifndef OPENSHOT_QIMAGE_READER_H
#define OPENSHOT_QIMAGE_READER_H

#include <memory>
#include <string>

#include <QString>
#include <QSize>

#include "ReaderBase.h"
#include "Json.h"

#if USE_RESVG == 1
	// If defined and found in CMake, utilize the libresvg for parsing
	// SVG files and rasterizing them to QImages.
	#include "ResvgQt.h"

    #define RESVG_VERSION_MIN(a, b) (\
        RESVG_MAJOR_VERSION > a \
        || (RESVG_MAJOR_VERSION == a && RESVG_MINOR_VERSION >= b) \
    )
#else
    #define RESVG_VERSION_MIN(a, b) 0
#endif

class QImage;

#include <QImage>
#include <QSize>
#include <QString>

#include <QSize>
#include <QString>

class QImage;

namespace openshot
{
    // Forward decl
    class CacheBase;
    class Frame;

	/**
	 * @brief This class uses the Qt library, to open image files, and return
	 * openshot::Frame objects containing the image.
	 *
	 * @code
	 * // Create a reader for a video
	 * QtImageReader r("MyAwesomeImage.jpeg");
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
	class QtImageReader : public ReaderBase
	{
	private:
		QString path;
		std::shared_ptr<QImage> image;			///> Original image (full quality)
		std::shared_ptr<QImage> cached_image;	///> Scaled for performance
		bool is_open;	///> Is Reader opened
		QSize max_size;	///> Current max_size as calculated with Clip properties

#if RESVG_VERSION_MIN(0, 11)
        ResvgOptions resvg_options;
#endif

		/// Load an SVG file with Resvg or fallback with Qt
        ///
        /// @returns Success as a boolean
        /// @param path The file path of the SVG file
        QSize load_svg_path(QString path);

		/// Calculate the max_size QSize, based on parent timeline and parent clip settings
        QSize calculate_max_size();

	public:
		/// @brief Constructor for QtImageReader.
		///
		/// Opens the media file to inspect its properties and loads frame 1,
		/// iff inspect_reader == true (the default). Pass a false value in
		/// the optional parameter to defer this initial Open()/Close() cycle.
		///
		/// When not inspecting the media file, it's much faster, and useful
		/// when you are inflating the object using JSON after instantiation.
		QtImageReader(std::string path, bool inspect_reader=true);

		virtual ~QtImageReader();

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
		std::string Name() override { return "QtImageReader"; };

		// Get and Set JSON methods
		std::string Json() const override; ///< Generate JSON string of this object
		void SetJson(const std::string value) override; ///< Load JSON string into this object
		Json::Value JsonValue() const override; ///< Generate Json::Value for this object
		void SetJsonValue(const Json::Value root) override; ///< Load Json::Value into this object

		/// Open File - which is called by the constructor automatically
		void Open() override;
	};

}

#endif
