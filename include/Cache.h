/**
 * @file
 * @brief Header file for Cache class
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

#ifndef OPENSHOT_CACHE_H
#define OPENSHOT_CACHE_H

#include <map>
#include <deque>
#include <tr1/memory>
#include "Frame.h"
#include "Exceptions.h"

namespace openshot {

//	struct ReaderInfo
//	{
//		int height;			///< The height of the video (in pixels)
//		int width;			///< The width of the video (in pixesl)
//		int sample_rate;	///< The number of audio samples per second (44100 is a common sample rate)
//		int channels;		///< The number of audio channels used in the audio stream
//	};

	/**
	 * @brief This class is a cache manager for Frame objects.
	 *
	 * It is used by FileReaders (such as FFmpegReader) to cache recently accessed frames. Due to the
	 * high cost of decoding streams, once a frame is decoded, converted to RGB, and a Frame object is created,
	 * it critical to keep these Frames cached for performance reasons.  However, the larger the cache, the more memory
	 * is required.  You can set the max number of bytes to cache.
	 */
	class Cache
	{
	private:
		int64 max_bytes;			///< This is the max number of bytes to cache (0 = no limit)
		map<int, tr1::shared_ptr<Frame> > frames;	///< This map holds the frame number and Frame objects
		deque<int> frame_numbers;	///< This queue holds a sequential list of cached Frame numbers

		/// Clean up cached frames that exceed the max number of bytes
		void CleanUp();

		/// Section lock for multiple threads
	    CriticalSection *cacheCriticalSection;


	public:
		/// Default constructor, no max bytes
		Cache();

		/// @brief Constructor that sets the max bytes to cache
		/// @param max_bytes The maximum bytes to allow in the cache. Once exceeded, the cache will purge the oldest frames.
		Cache(int64 max_bytes);

		// Default destructor
		~Cache();

		/// @brief Add a Frame to the cache
		/// @param frame_number The frame number of the cached frame
		/// @param frame The openshot::Frame object needing to be cached.
		void Add(int frame_number, tr1::shared_ptr<Frame> frame);

		/// Clear the cache of all frames
		void Clear();

		/// Count the frames in the queue
		int Count();

		/// Display a list of cached frame numbers
		void Display();

		/// @brief Get a frame from the cache
		/// @param frame_number The frame number of the cached frame
		tr1::shared_ptr<Frame> GetFrame(int frame_number);

		/// Gets the maximum bytes value
		int64 GetBytes();

		/// Gets the maximum bytes value
		int64 GetMaxBytes() { return max_bytes; };

		/// Get the smallest frame number
		tr1::shared_ptr<Frame> GetSmallestFrame();

		/// @brief Move frame to front of queue (so it lasts longer)
		/// @param frame_number The frame number of the cached frame
		void MoveToFront(int frame_number);

		/// @brief Remove a specific frame
		/// @param frame_number The frame number of the cached frame
		void Remove(int frame_number);

		/// @brief Set maximum bytes to a different amount
		/// @param number_of_bytes The maximum bytes to allow in the cache. Once exceeded, the cache will purge the oldest frames.
		void SetMaxBytes(int64 number_of_bytes) { max_bytes = number_of_bytes; CleanUp(); };

		/// @brief Set maximum bytes to a different amount based on a ReaderInfo struct
		/// @param number_of_frames The maximum number of frames to hold in cache
		/// @param width The width of the frame's image
		/// @param height The height of the frame's image
		/// @param sample_rate The sample rate of the frame's audio data
		/// @param channels The number of audio channels in the frame
		void SetMaxBytesFromInfo(int number_of_frames, int width, int height, int sample_rate, int channels);


	};

}

#endif
