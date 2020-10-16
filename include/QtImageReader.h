/**
 * @file
 * @brief Header file for QtImageReader class
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

#ifndef OPENSHOT_QIMAGE_READER_H
#define OPENSHOT_QIMAGE_READER_H

#include <cmath>
#include <ctime>
#include <iostream>
#include <omp.h>
#include <stdio.h>
#include <memory>
#include "Exceptions.h"
#include "ReaderBase.h"

namespace openshot
{

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

	public:

		/// Constructor for QtImageReader.  This automatically opens the media file and loads
		/// frame 1, or it throws one of the following exceptions.
		QtImageReader(std::string path);

		/// Constructor for QtImageReader.  This only opens the media file to inspect its properties
		/// if inspect_reader=true. When not inspecting the media file, it's much faster, and useful
		/// when you are inflating the object using JSON after instantiating it.
		QtImageReader(std::string path, bool inspect_reader);

		virtual ~QtImageReader();

		/// Close File
		void Close() override;

		/// Get the cache object used by this reader (always returns NULL for this object)
		CacheMemory* GetCache() override { return NULL; };

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

		/// Get and Set JSON methods
		std::string Json() const override; ///< Generate JSON string of this object
		void SetJson(const std::string value) override; ///< Load JSON string into this object
		Json::Value JsonValue() const override; ///< Generate Json::Value for this object
		void SetJsonValue(const Json::Value root) override; ///< Load Json::Value into this object

		/// Open File - which is called by the constructor automatically
		void Open() override;
	};

}

#endif
