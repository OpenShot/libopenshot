/**
 * @file
 * @brief Header file for CacheBase class
 * @author Jonathan Thomas <jonathan@openshot.org>
 *
 * @ref License
 */

// Copyright (c) 2008-2019 OpenShot Studios, LLC
//
// SPDX-License-Identifier: LGPL-3.0-or-later

#ifndef OPENSHOT_CACHE_BASE_H
#define OPENSHOT_CACHE_BASE_H

#include <map>
#include <vector>
#include <memory>
#include <mutex>
#include <algorithm>

#include "Json.h"

namespace openshot {
	class Frame;

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
		std::string cache_type; ///< This is a friendly type name of the derived cache instance
		int64_t max_bytes; ///< This is the max number of bytes to cache (0 = no limit)

		bool needs_range_processing; ///< Something has changed, and the range data needs to be re-calculated
		std::string json_ranges; ///< JSON ranges of frame numbers
		std::vector<int64_t> ordered_frame_numbers; ///< Ordered list of frame numbers used by cache
		std::map<int64_t, int64_t> frame_ranges;	///< This map holds the ranges of frames, useful for quickly displaying the contents of the cache
		int64_t range_version; ///< The version of the JSON range data (incremented with each change)
        
		/// Mutex for multiple threads
		std::recursive_mutex *cacheMutex;

		/// Calculate ranges of frames
		void CalculateRanges();

	public:
		/// Default constructor, no max bytes
		CacheBase();

		/// @brief Constructor that sets the max bytes to cache
		/// @param max_bytes The maximum bytes to allow in the cache. Once exceeded, the cache will purge the oldest frames.
		CacheBase(int64_t max_bytes);

		/// @brief Add a Frame to the cache
		/// @param frame The openshot::Frame object needing to be cached.
		virtual void Add(std::shared_ptr<openshot::Frame> frame) = 0;

		/// Clear the cache of all frames
		virtual void Clear() = 0;

		/// @brief Check if frame is already contained in cache
		/// @param frame_number The frame number to be checked
		virtual bool Contains(int64_t frame_number) = 0;

		/// Count the frames in the queue
		virtual int64_t Count() = 0;

		/// @brief Get a frame from the cache
		/// @param frame_number The frame number of the cached frame
		virtual std::shared_ptr<openshot::Frame> GetFrame(int64_t frame_number) = 0;

		/// @brief Get an vector of all Frames
		virtual std::vector<std::shared_ptr<openshot::Frame>> GetFrames() = 0;

		/// Gets the maximum bytes value
		virtual int64_t GetBytes() = 0;

		/// Get the smallest frame number
		virtual std::shared_ptr<openshot::Frame> GetSmallestFrame() = 0;

		/// @brief Remove a specific frame
		/// @param frame_number The frame number of the cached frame
		virtual void Remove(int64_t frame_number) = 0;

		/// @brief Remove a range of frames
		/// @param start_frame_number The starting frame number of the cached frame
		/// @param end_frame_number The ending frame number of the cached frame
		virtual void Remove(int64_t start_frame_number, int64_t end_frame_number) = 0;

		/// @brief Move frame to front of queue (so it lasts longer)
		/// @param frame_number The frame number of the cached frame
		virtual void Touch(int64_t frame_number) = 0;

		/// Gets the maximum bytes value
		int64_t GetMaxBytes() { return max_bytes; };

		/// @brief Set maximum bytes to a different amount
		/// @param number_of_bytes The maximum bytes to allow in the cache. Once exceeded, the cache will purge the oldest frames.
		void SetMaxBytes(int64_t number_of_bytes) { max_bytes = number_of_bytes; };

		/// @brief Set maximum bytes to a different amount based on a ReaderInfo struct
		/// @param number_of_frames The maximum number of frames to hold in cache
		/// @param width The width of the frame's image
		/// @param height The height of the frame's image
		/// @param sample_rate The sample rate of the frame's audio data
		/// @param channels The number of audio channels in the frame
		void SetMaxBytesFromInfo(int64_t number_of_frames, int width, int height, int sample_rate, int channels);

		// Get and Set JSON methods
		virtual std::string Json() = 0; ///< Generate JSON string of this object
		virtual void SetJson(const std::string value) = 0; ///< Load JSON string into this object
		virtual Json::Value JsonValue() = 0; ///< Generate Json::Value for this object
		virtual void SetJsonValue(const Json::Value root) = 0; ///< Load Json::Value into this object
		virtual ~CacheBase() = default;

	};

}

#endif
