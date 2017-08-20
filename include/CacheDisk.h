/**
 * @file
 * @brief Header file for CacheDisk class
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

#ifndef OPENSHOT_CACHE_DISK_H
#define OPENSHOT_CACHE_DISK_H

#include <map>
#include <deque>
#include <memory>
#include "CacheBase.h"
#include "Frame.h"
#include "Exceptions.h"
#include <QDir>
#include <QString>
#include <QTextStream>

namespace openshot {

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
		map<long int, long int> frames;	///< This map holds the frame number and Frame objects
		deque<long int> frame_numbers;	///< This queue holds a sequential list of cached Frame numbers
		string image_format;
		float image_quality;
		float image_scale;

		long long int frame_size_bytes; ///< The size of the cached frame in bytes
		bool needs_range_processing; ///< Something has changed, and the range data needs to be re-calculated
		string json_ranges; ///< JSON ranges of frame numbers
		vector<long int> ordered_frame_numbers; ///< Ordered list of frame numbers used by cache
		map<long int, long int> frame_ranges;	///< This map holds the ranges of frames, useful for quickly displaying the contents of the cache
		long int range_version; ///< The version of the JSON range data (incremented with each change)

		/// Clean up cached frames that exceed the max number of bytes
		void CleanUp();

		/// Init path directory
		void InitPath(string cache_path);

		/// Calculate ranges of frames
		void CalculateRanges();

	public:
		/// @brief Default constructor, no max bytes
		/// @param cache_path The folder path of the cache directory (empty string = /tmp/preview-cache/)
		/// @param format The image format for disk caching (ppm, jpg, png)
		/// @param quality The quality of the image (1.0=highest quality/slowest speed, 0.0=worst quality/fastest speed)
		/// @param scale The scale factor for the preview images (1.0 = original size, 0.5=half size, 0.25=quarter size, etc...)
		CacheDisk(string cache_path, string format, float quality, float scale);

		/// @brief Constructor that sets the max bytes to cache
		/// @param cache_path The folder path of the cache directory (empty string = /tmp/preview-cache/)
		/// @param format The image format for disk caching (ppm, jpg, png)
		/// @param quality The quality of the image (1.0=highest quality/slowest speed, 0.0=worst quality/fastest speed)
		/// @param scale The scale factor for the preview images (1.0 = original size, 0.5=half size, 0.25=quarter size, etc...)
		/// @param max_bytes The maximum bytes to allow in the cache. Once exceeded, the cache will purge the oldest frames.
		CacheDisk(string cache_path, string format, float quality, float scale, long long int max_bytes);

		// Default destructor
		~CacheDisk();

		/// @brief Add a Frame to the cache
		/// @param frame The openshot::Frame object needing to be cached.
		void Add(std::shared_ptr<Frame> frame);

		/// Clear the cache of all frames
		void Clear();

		/// Count the frames in the queue
		long int Count();

		/// @brief Get a frame from the cache
		/// @param frame_number The frame number of the cached frame
		std::shared_ptr<Frame> GetFrame(long int frame_number);

		/// Gets the maximum bytes value
		long long int GetBytes();

		/// Get the smallest frame number
		std::shared_ptr<Frame> GetSmallestFrame();

		/// @brief Move frame to front of queue (so it lasts longer)
		/// @param frame_number The frame number of the cached frame
		void MoveToFront(long int frame_number);

		/// @brief Remove a specific frame
		/// @param frame_number The frame number of the cached frame
		void Remove(long int frame_number);

		/// @brief Remove a range of frames
		/// @param start_frame_number The starting frame number of the cached frame
		/// @param end_frame_number The ending frame number of the cached frame
		void Remove(long int start_frame_number, long int end_frame_number);

		/// Get and Set JSON methods
		string Json(); ///< Generate JSON string of this object
		void SetJson(string value) throw(InvalidJSON); ///< Load JSON string into this object
		Json::Value JsonValue(); ///< Generate Json::JsonValue for this object
		void SetJsonValue(Json::Value root) throw(InvalidFile, ReaderClosed); ///< Load Json::JsonValue into this object
	};

}

#endif
