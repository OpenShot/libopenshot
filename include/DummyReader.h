/**
 * @file
 * @brief Header file for DummyReader class
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

#ifndef OPENSHOT_DUMMY_READER_H
#define OPENSHOT_DUMMY_READER_H

#include "ReaderBase.h"

#include <cmath>
#include <ctime>
#include <iostream>
#include <omp.h>
#include <stdio.h>
#include <memory>
#include "CacheMemory.h"
#include "Exceptions.h"
#include "Fraction.h"

using namespace std;

namespace openshot
{
	/**
	 * @brief This class is used as a simple, dummy reader, which always returns a blank frame.
	 *
	 * A dummy reader can be created with any framerate or samplerate. This is useful in unit
	 * tests that need to test different framerates or samplerates.
	 */
	class DummyReader : public ReaderBase
	{
	private:
		std::shared_ptr<Frame> image_frame;
		bool is_open;

	public:

		/// Blank constructor for DummyReader, with default settings.
		DummyReader();

		/// Constructor for DummyReader.
		DummyReader(Fraction fps, int width, int height, int sample_rate, int channels, float duration);

		/// Close File
		void Close();

		/// Get the cache object used by this reader (always returns NULL for this reader)
		CacheMemory* GetCache() { return NULL; };

		/// Get an openshot::Frame object for a specific frame number of this reader.  All numbers
		/// return the same Frame, since they all share the same image data.
		///
		/// @returns The requested frame (containing the image)
		/// @param requested_frame The frame number that is requested.
		std::shared_ptr<Frame> GetFrame(long int requested_frame) throw(ReaderClosed);

		/// Determine if reader is open or closed
		bool IsOpen() { return is_open; };

		/// Return the type name of the class
		string Name() { return "DummyReader"; };

		/// Get and Set JSON methods
		string Json(); ///< Generate JSON string of this object
		void SetJson(string value) throw(InvalidJSON); ///< Load JSON string into this object
		Json::Value JsonValue(); ///< Generate Json::JsonValue for this object
		void SetJsonValue(Json::Value root) throw(InvalidFile); ///< Load Json::JsonValue into this object

		/// Open File - which is called by the constructor automatically
		void Open() throw(InvalidFile);
	};

}

#endif
