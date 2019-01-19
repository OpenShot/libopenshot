/**
 * @file
 * @brief Header file for QtImageReader class
 * @author Jonathan Thomas <jonathan@openshot.org>
 *
 * @section LICENSE
 *
 * Copyright (c) 2008-2014 OpenShot Studios, LLC
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

using namespace std;

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
		string path;
		std::shared_ptr<QImage> image; ///> Original image (full quality)
		std::shared_ptr<QImage> cached_image; ///> Scaled for performance
		bool is_open;

	public:

		/// Constructor for QtImageReader.  This automatically opens the media file and loads
		/// frame 1, or it throws one of the following exceptions.
		QtImageReader(string path);

		/// Constructor for QtImageReader.  This only opens the media file to inspect it's properties
		/// if inspect_reader=true. When not inspecting the media file, it's much faster, and useful
		/// when you are inflating the object using JSON after instantiating it.
		QtImageReader(string path, bool inspect_reader);

		/// Close File
		void Close();

		/// Get the cache object used by this reader (always returns NULL for this object)
		CacheMemory* GetCache() { return NULL; };

		/// Get an openshot::Frame object for a specific frame number of this reader.  All numbers
		/// return the same Frame, since they all share the same image data.
		///
		/// @returns The requested frame (containing the image)
		/// @param requested_frame The frame number that is requested.
		std::shared_ptr<Frame> GetFrame(int64_t requested_frame);

		/// Determine if reader is open or closed
		bool IsOpen() { return is_open; };

		/// Return the type name of the class
		string Name() { return "QtImageReader"; };

		/// Get and Set JSON methods
		string Json(); ///< Generate JSON string of this object
		void SetJson(string value); ///< Load JSON string into this object
		Json::Value JsonValue(); ///< Generate Json::JsonValue for this object
		void SetJsonValue(Json::Value root); ///< Load Json::JsonValue into this object

		/// Open File - which is called by the constructor automatically
		void Open();
	};

}

#endif
