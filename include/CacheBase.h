/**
 * @file
 * @brief Header file for CacheBase class
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

#ifndef OPENSHOT_CACHE_BASE_H
#define OPENSHOT_CACHE_BASE_H

#include <memory>
#include "Frame.h"
#include "Exceptions.h"
#include "Json.h"

namespace openshot {

	/**
	 * @brief All cache managers in libopenshot are based on this CacheBase class
	 *
	 * Cache is a very important element of video editing, and is required to achieve a high degree
	 * of performance. There are multiple derived cache objects based on this class, some which use
	 * memory, and some which use disk to store the cache.
	 */
	class CacheBase
	{
	protected:
		string cache_type; ///< This is a friendly type name of the derived cache instance
		long long int max_bytes; ///< This is the max number of bytes to cache (0 = no limit)

		/// Section lock for multiple threads
	    CriticalSection *cacheCriticalSection;


	public:
		/// Default constructor, no max bytes
		CacheBase();

		/// @brief Constructor that sets the max bytes to cache
		/// @param max_bytes The maximum bytes to allow in the cache. Once exceeded, the cache will purge the oldest frames.
		CacheBase(long long int max_bytes);

		/// @brief Add a Frame to the cache
		/// @param frame The openshot::Frame object needing to be cached.
		virtual void Add(std::shared_ptr<Frame> frame) = 0;

		/// Clear the cache of all frames
		virtual void Clear() = 0;

		/// Count the frames in the queue
		virtual long int Count() = 0;

		/// @brief Get a frame from the cache
		/// @param frame_number The frame number of the cached frame
		virtual std::shared_ptr<Frame> GetFrame(long int frame_number) = 0;

		/// Gets the maximum bytes value
		virtual long long int GetBytes() = 0;

		/// Get the smallest frame number
		virtual std::shared_ptr<Frame> GetSmallestFrame() = 0;

		/// @brief Remove a specific frame
		/// @param frame_number The frame number of the cached frame
		virtual void Remove(long int frame_number) = 0;

		/// @brief Remove a range of frames
		/// @param start_frame_number The starting frame number of the cached frame
		/// @param end_frame_number The ending frame number of the cached frame
		virtual void Remove(long int start_frame_number, long int end_frame_number) = 0;

		/// Gets the maximum bytes value
		long long int GetMaxBytes() { return max_bytes; };

		/// @brief Set maximum bytes to a different amount
		/// @param number_of_bytes The maximum bytes to allow in the cache. Once exceeded, the cache will purge the oldest frames.
		void SetMaxBytes(long long int number_of_bytes) { max_bytes = number_of_bytes; };

		/// @brief Set maximum bytes to a different amount based on a ReaderInfo struct
		/// @param number_of_frames The maximum number of frames to hold in cache
		/// @param width The width of the frame's image
		/// @param height The height of the frame's image
		/// @param sample_rate The sample rate of the frame's audio data
		/// @param channels The number of audio channels in the frame
		void SetMaxBytesFromInfo(long int number_of_frames, int width, int height, int sample_rate, int channels);

		/// Get and Set JSON methods
		virtual string Json() = 0; ///< Generate JSON string of this object
		virtual void SetJson(string value) throw(InvalidJSON) = 0; ///< Load JSON string into this object
		virtual Json::Value JsonValue() = 0; ///< Generate Json::JsonValue for this object
		virtual void SetJsonValue(Json::Value root) = 0; ///< Load Json::JsonValue into this object

	};

}

#endif
