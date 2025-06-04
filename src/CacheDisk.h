/**
 * @file
 * @brief Header file for CacheDisk class
 * @author Jonathan Thomas <jonathan@openshot.org>
 *
 * @ref License
 */

// Copyright (c) 2008-2019 OpenShot Studios, LLC
//
// SPDX-License-Identifier: LGPL-3.0-or-later

#ifndef OPENSHOT_CACHE_DISK_H
#define OPENSHOT_CACHE_DISK_H

#include "CacheBase.h"

#include <QDir>

namespace openshot {
	class Frame;

	/**
	 * @brief This class is a disk-based cache manager for Frame objects.
	 *
	 * It is used by the Timeline class, if enabled, to cache video and audio frames to disk, to cut down on CPU
	 * and memory utilization. This will thrash a user's disk, but save their memory and CPU. It's a trade off that
	 * sometimes makes perfect sense. You can also set the max number of bytes to cache.
	 */
	class CacheDisk : public CacheBase {
	private:
		QDir path; ///< This is the folder path of the cache directory
		std::map<int64_t, int64_t> frames;	///< This map holds the frame number and Frame objects
		std::deque<int64_t> frame_numbers;	///< This queue holds a sequential list of cached Frame numbers
		std::string image_format;
		float image_quality;
		float image_scale;
		int64_t frame_size_bytes; ///< The size of the cached frame in bytes

		/// Clean up cached frames that exceed the max number of bytes
		void CleanUp();

		/// Init path directory
		void InitPath(std::string cache_path);

	public:
		/// @brief Default constructor, no max bytes
		/// @param cache_path The folder path of the cache directory (empty string = /tmp/preview-cache/)
		/// @param format The image format for disk caching (ppm, jpg, png)
		/// @param quality The quality of the image (1.0=highest quality/slowest speed, 0.0=worst quality/fastest speed)
		/// @param scale The scale factor for the preview images (1.0 = original size, 0.5=half size, 0.25=quarter size, etc...)
		CacheDisk(std::string cache_path, std::string format, float quality, float scale);

		/// @brief Constructor that sets the max bytes to cache
		/// @param cache_path The folder path of the cache directory (empty string = /tmp/preview-cache/)
		/// @param format The image format for disk caching (ppm, jpg, png)
		/// @param quality The quality of the image (1.0=highest quality/slowest speed, 0.0=worst quality/fastest speed)
		/// @param scale The scale factor for the preview images (1.0 = original size, 0.5=half size, 0.25=quarter size, etc...)
		/// @param max_bytes The maximum bytes to allow in the cache. Once exceeded, the cache will purge the oldest frames.
		CacheDisk(std::string cache_path, std::string format, float quality, float scale, int64_t max_bytes);

		// Default destructor
		~CacheDisk();

		/// @brief Add a Frame to the cache
		/// @param frame The openshot::Frame object needing to be cached.
		void Add(std::shared_ptr<openshot::Frame> frame);

		/// Clear the cache of all frames
		void Clear();

		/// @brief Check if frame is already contained in cache
		/// @param frame_number The frame number to be checked
		bool Contains(int64_t frame_number);

		/// Count the frames in the queue
		int64_t Count();

		/// @brief Get a frame from the cache
		/// @param frame_number The frame number of the cached frame
		std::shared_ptr<openshot::Frame> GetFrame(int64_t frame_number);

		/// @brief Get an array of all Frames
		std::vector<std::shared_ptr<openshot::Frame>> GetFrames();

		/// Gets the maximum bytes value
		int64_t GetBytes();

		/// Get the smallest frame number
		std::shared_ptr<openshot::Frame> GetSmallestFrame();

		/// @brief Move frame to front of queue (so it lasts longer)
		/// @param frame_number The frame number of the cached frame
		void Touch(int64_t frame_number);

		/// @brief Remove a specific frame
		/// @param frame_number The frame number of the cached frame
		void Remove(int64_t frame_number);

		/// @brief Remove a range of frames
		/// @param start_frame_number The starting frame number of the cached frame
		/// @param end_frame_number The ending frame number of the cached frame
		void Remove(int64_t start_frame_number, int64_t end_frame_number);

		// Get and Set JSON methods
		std::string Json(); ///< Generate JSON string of this object
		void SetJson(const std::string value); ///< Load JSON string into this object
		Json::Value JsonValue(); ///< Generate Json::Value for this object
		void SetJsonValue(const Json::Value root); ///< Load Json::Value into this object
	};

}

#endif
