/**
 * @file
 * @brief Header file for Cache class
 * @author Jonathan Thomas <jonathan@openshot.org>
 *
 * @section LICENSE
 *
 * Copyright (c) 2008-2013 OpenShot Studios, LLC
 * (http://www.openshotstudios.com). This file is part of
 * OpenShot Library (http://www.openshot.org), an open-source project
 * dedicated to delivering high quality video editing and animation solutions
 * to the world.
 *
 * OpenShot Library is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * OpenShot Library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with OpenShot Library.  If not, see <http://www.gnu.org/licenses/>.
 */

#ifndef OPENSHOT_CACHE_H
#define OPENSHOT_CACHE_H

#include <map>
#include <deque>
#include <tr1/memory>
#include "Frame.h"
#include "Exceptions.h"

/// This namespace is the default namespace for all code in the openshot library.
namespace openshot {

	/**
	 * @brief This class is a cache manager for Frame objects.  It is used by FileReaders (such as FFmpegReader) to cache
	 * recently accessed frames.
	 *
	 * Due to the high cost of decoding streams, once a frame is decoded, converted to RGB, and a Frame object is created,
	 * it critical to keep these Frames cached for performance reasons.  However, the larger the cache, the more memory
	 * is required.  You can set the max number of bytes to cache.
	 */
	class Cache {
	private:
		int64 total_bytes;			///< This is the current total bytes (that are in this cache)
		int64 max_bytes;			///< This is the max number of bytes to cache (0 = no limit)
		map<int, tr1::shared_ptr<Frame> > frames;	///< This map holds the frame number and Frame objects
		deque<int> frame_numbers;	///< This queue holds a sequential list of cached Frame numbers

		/// Clean up cached frames that exceed the max number of bytes
		void CleanUp();

	public:
		/// Default constructor, no max bytes
		Cache();

		/// Constructor that sets the max bytes to cache
		Cache(int64 max_bytes);

		/// Add a Frame to the cache
		void Add(int frame_number, tr1::shared_ptr<Frame> frame);

		/// Clear the cache of all frames
		void Clear();

		/// Count the frames in the queue
		int Count();

		/// Display a list of cached frame numbers
		void Display();

		/// Check for the existence of a frame in the cache
		bool Exists(int frame_number);

		/// Get a frame from the cache
		tr1::shared_ptr<Frame> GetFrame(int frame_number);

		/// Gets the maximum bytes value
		int64 GetBytes() { return total_bytes; };

		/// Gets the maximum bytes value
		int64 GetMaxBytes() { return max_bytes; };

		/// Get the smallest frame number
		tr1::shared_ptr<Frame> GetSmallestFrame();

		/// Move frame to front of queue (so it lasts longer)
		void MoveToFront(int frame_number);

		/// Remove a specific frame
		void Remove(int frame_number);

		/// Set maximum bytes to a different amount
		void SetMaxBytes(int64 number_of_bytes) { max_bytes = number_of_bytes; CleanUp(); };


	};

}

#endif
