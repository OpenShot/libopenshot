/**
 * @file
 * @brief Header file for CacheMemory class
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

#ifndef OPENSHOT_CACHE_MEMORY_H
#define OPENSHOT_CACHE_MEMORY_H

#include <map>
#include <deque>
#include <memory>
#include "CacheBase.h"
#include "Frame.h"
#include "Exceptions.h"

namespace openshot {

	/**
	 * @brief This class is a memory-based cache manager for Frame objects.
	 *
	 * It is used by FileReaders (such as FFmpegReader) to cache recently accessed frames. Due to the
	 * high cost of decoding streams, once a frame is decoded, converted to RGB, and a Frame object is created,
	 * it critical to keep these Frames cached for performance reasons.  However, the larger the cache, the more memory
	 * is required.  You can set the max number of bytes to cache.
	 */
	class CacheMemory : public CacheBase {
	private:
		std::map<int64_t, std::shared_ptr<openshot::Frame> > frames;	///< This map holds the frame number and Frame objects
		std::deque<int64_t> frame_numbers;	///< This queue holds a sequential list of cached Frame numbers

		bool needs_range_processing; ///< Something has changed, and the range data needs to be re-calculated
		std::string json_ranges; ///< JSON ranges of frame numbers
		std::vector<int64_t> ordered_frame_numbers; ///< Ordered list of frame numbers used by cache
		std::map<int64_t, int64_t> frame_ranges;	///< This map holds the ranges of frames, useful for quickly displaying the contents of the cache
		int64_t range_version; ///< The version of the JSON range data (incremented with each change)

		/// Clean up cached frames that exceed the max number of bytes
		void CleanUp();

		/// Calculate ranges of frames
		void CalculateRanges();

	public:
		/// Default constructor, no max bytes
		CacheMemory();

		/// @brief Constructor that sets the max bytes to cache
		/// @param max_bytes The maximum bytes to allow in the cache. Once exceeded, the cache will purge the oldest frames.
		CacheMemory(int64_t max_bytes);

		// Default destructor
		virtual ~CacheMemory();

		/// @brief Add a Frame to the cache
		/// @param frame The openshot::Frame object needing to be cached.
		void Add(std::shared_ptr<openshot::Frame> frame);

		/// Clear the cache of all frames
		void Clear();

		/// Count the frames in the queue
		int64_t Count();

		/// @brief Get a frame from the cache
		/// @param frame_number The frame number of the cached frame
		std::shared_ptr<openshot::Frame> GetFrame(int64_t frame_number);

		/// Gets the maximum bytes value
		int64_t GetBytes();

		/// Get the smallest frame number
		std::shared_ptr<openshot::Frame> GetSmallestFrame();

		/// @brief Move frame to front of queue (so it lasts longer)
		/// @param frame_number The frame number of the cached frame
		void MoveToFront(int64_t frame_number);

		/// @brief Remove a specific frame
		/// @param frame_number The frame number of the cached frame
		void Remove(int64_t frame_number);

		/// @brief Remove a range of frames
		/// @param start_frame_number The starting frame number of the cached frame
		/// @param end_frame_number The ending frame number of the cached frame
		void Remove(int64_t start_frame_number, int64_t end_frame_number);

		/// Get and Set JSON methods
		std::string Json(); ///< Generate JSON string of this object
		void SetJson(const std::string value); ///< Load JSON string into this object
		Json::Value JsonValue(); ///< Generate Json::Value for this object
		void SetJsonValue(const Json::Value root); ///< Load Json::Value into this object
	};

}

#endif
