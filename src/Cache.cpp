/**
 * @file
 * @brief Source file for Cache class
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

#include "../include/Cache.h"

using namespace std;
using namespace openshot;

// Default constructor, no max frames
Cache::Cache() : max_bytes(0) {
	// Init the critical section
	cacheCriticalSection = new CriticalSection();
};

// Constructor that sets the max frames to cache
Cache::Cache(int64 max_bytes) : max_bytes(max_bytes) {
	// Init the critical section
	cacheCriticalSection = new CriticalSection();
};

// Default destructor
Cache::~Cache()
{
	frames.clear();
	frame_numbers.clear();

	// remove critical section
	delete cacheCriticalSection;
	cacheCriticalSection = NULL;
}

// Add a Frame to the cache
void Cache::Add(long int frame_number, tr1::shared_ptr<Frame> frame)
{
	// Create a scoped lock, to protect the cache from multiple threads
	const GenericScopedLock<CriticalSection> lock(*cacheCriticalSection);

	// Remove frame if it already exists
	if (frames.count(frame_number))
		// Move frame to front of queue
		MoveToFront(frame_number);

	else
	{
		// Add frame to queue and map
		frames[frame_number] = frame;
		frame_numbers.push_front(frame_number);

		// Clean up old frames
		CleanUp();
	}
}

// Get a frame from the cache (or NULL shared_ptr if no frame is found)
tr1::shared_ptr<Frame> Cache::GetFrame(long int frame_number)
{
	// Create a scoped lock, to protect the cache from multiple threads
	const GenericScopedLock<CriticalSection> lock(*cacheCriticalSection);

	// Does frame exists in cache?
	if (frames.count(frame_number))
		// return the Frame object
		return frames[frame_number];

	else
		// no Frame found
		return tr1::shared_ptr<Frame>();
}

// Return a deque of all frame numbers in this queue (returns just a copy of the data)
deque<long int> Cache::GetFrameNumbers() {

	// Create a scoped lock, to protect the cache from multiple threads
	const GenericScopedLock<CriticalSection> lock(*cacheCriticalSection);

	// Make copy of deque
	deque<long int> copy_frame_numbers;

	// Loop through frame numbers
	deque<long int>::iterator itr;
	for(itr = frame_numbers.begin(); itr != frame_numbers.end(); ++itr)
		copy_frame_numbers.push_back(*itr);

	return copy_frame_numbers;
}

// Get the smallest frame number (or NULL shared_ptr if no frame is found)
tr1::shared_ptr<Frame> Cache::GetSmallestFrame()
{
	// Create a scoped lock, to protect the cache from multiple threads
	const GenericScopedLock<CriticalSection> lock(*cacheCriticalSection);
	tr1::shared_ptr<openshot::Frame> f;

	 // Loop through frame numbers
	 deque<long int>::iterator itr;
	 long int smallest_frame = -1;
	 for(itr = frame_numbers.begin(); itr != frame_numbers.end(); ++itr)
	 {
		 if (*itr < smallest_frame || smallest_frame == -1)
			 smallest_frame = *itr;
	 }

	 // Return frame
	 f = GetFrame(smallest_frame);

	 return f;
}

// Gets the maximum bytes value
int64 Cache::GetBytes()
{
	// Create a scoped lock, to protect the cache from multiple threads
	const GenericScopedLock<CriticalSection> lock(*cacheCriticalSection);

	int64 total_bytes = 0;

	// Loop through frames, and calculate total bytes
	deque<long int>::reverse_iterator itr;
	for(itr = frame_numbers.rbegin(); itr != frame_numbers.rend(); ++itr)
	{
		total_bytes += frames[*itr]->GetBytes();
	}

	return total_bytes;
}

// Remove a specific frame
void Cache::Remove(long int frame_number)
{
	// Create a scoped lock, to protect the cache from multiple threads
	const GenericScopedLock<CriticalSection> lock(*cacheCriticalSection);

	// Loop through frame numbers
	deque<long int>::iterator itr;
	for(itr = frame_numbers.begin(); itr != frame_numbers.end(); ++itr)
	{
		if (*itr == frame_number)
		{
			// erase frame number
			frame_numbers.erase(itr);
			break;
		}
	}

	// Remove frame from map
	frames.erase(frame_number);
}

// Move frame to front of queue (so it lasts longer)
void Cache::MoveToFront(long int frame_number)
{
	// Create a scoped lock, to protect the cache from multiple threads
	const GenericScopedLock<CriticalSection> lock(*cacheCriticalSection);

	// Does frame exists in cache?
	if (frames.count(frame_number))
	{
		// Loop through frame numbers
		deque<long int>::iterator itr;
		for(itr = frame_numbers.begin(); itr != frame_numbers.end(); ++itr)
		{
			if (*itr == frame_number)
			{
				// erase frame number
				frame_numbers.erase(itr);

				// add frame number to 'front' of queue
				frame_numbers.push_front(frame_number);
				break;
			}
		}
	}
}

// Clear the cache of all frames
void Cache::Clear()
{
	// Create a scoped lock, to protect the cache from multiple threads
	const GenericScopedLock<CriticalSection> lock(*cacheCriticalSection);

	frames.clear();
	frame_numbers.clear();
}

// Count the frames in the queue
long int Cache::Count()
{
	// Create a scoped lock, to protect the cache from multiple threads
	const GenericScopedLock<CriticalSection> lock(*cacheCriticalSection);

	// Return the number of frames in the cache
	return frames.size();
}

// Clean up cached frames that exceed the number in our max_bytes variable
void Cache::CleanUp()
{
	// Create a scoped lock, to protect the cache from multiple threads
	const GenericScopedLock<CriticalSection> lock(*cacheCriticalSection);

	// Do we auto clean up?
	if (max_bytes > 0)
	{
		while (GetBytes() > max_bytes && frame_numbers.size() > 20)
		{
			// Remove the oldest frame
			long int frame_to_remove = frame_numbers.back();

			// Remove frame_number and frame
			Remove(frame_to_remove);
		}
	}
}

// Display a list of cached frame numbers
void Cache::Display()
{
	cout << "----- Cache List (" << frames.size() << ") ------" << endl;
	deque<long int>::iterator itr;

	int i = 1;
	for(itr = frame_numbers.begin(); itr != frame_numbers.end(); ++itr)
	{
		cout << " " << i << ") --- Frame " << *itr << endl;
		i++;
	}
}

// Set maximum bytes to a different amount based on a ReaderInfo struct
void Cache::SetMaxBytesFromInfo(long int number_of_frames, int width, int height, int sample_rate, int channels)
{
	// n frames X height X width X 4 colors of chars X audio channels X 4 byte floats
	int64 bytes = number_of_frames * (height * width * 4 + (sample_rate * channels * 4));
	SetMaxBytes(bytes);
}

