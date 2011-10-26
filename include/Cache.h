#ifndef OPENSHOT_CACHE_H
#define OPENSHOT_CACHE_H

/**
 * \file
 * \brief Header file for Cache class
 * \author Copyright (c) 2011 Jonathan Thomas
 */

#include <map>
#include <deque>
#include "Frame.h"
#include "Exceptions.h"

/// This namespace is the default namespace for all code in the openshot library.
namespace openshot {

	/**
	 * \brief This class is a cache manager for Frame objects.  It is used by FileReaders (such as FFmpegReader) to cache
	 * recently accessed frames.
	 *
	 * Due to the high cost of decoding streams, once a frame is decoded, converted to RGB, and a Frame object is created,
	 * it critical to keep these Frames cached for performance reasons.
	 */
	class Cache {
	private:
		int max_frames;				///< This is the max number of frames to cache
		map<int, Frame> frames;		///< This map holds the frame number and Frame objects
		deque<int> frame_numbers;	///< This queue holds a sequential list of cached Frame numbers

		/// Clean up cached frames that exceed the number in our max_frames variable
		void CleanUp();

	public:
		/// Default constructor, max frames to cache is 20
		Cache();

		/// Constructor that sets the max frames to cache
		Cache(int max_frames);

		/// Add a Frame to the cache
		void Add(int frame_number, Frame frame);

		/// Check for the existance of a frame in the cache
		bool Exists(int frame_number);

		/// Get a frame from the cache
		Frame GetFrame(int frame_number);

		/// Get the smallest frame number
		Frame GetSmallestFrame();

		/// Remove a specific frame
		void Remove(int frame_number);

		/// Clear the cache of all frames
		void Clear();

		/// Display the list of cache and clear the cache (mainly for debugging reasons)
		void DisplayAndClear();

		/// Count the frames in the queue
		int Count();

	};

}

#endif
