/**
 * \file
 * \brief Source code for the Cache class
 * \author Copyright (c) 2011 Jonathan Thomas
 */

#include "../include/Cache.h"

using namespace std;
using namespace openshot;

// Default constructor, no max frames
Cache::Cache() : max_bytes(0), total_bytes(0) { };

// Constructor that sets the max frames to cache
Cache::Cache(int64 max_bytes) : max_bytes(max_bytes), total_bytes(0) { };

// Add a Frame to the cache
void Cache::Add(int frame_number, tr1::shared_ptr<Frame> frame)
{
	// Remove frame if it already exists
	if (Exists(frame_number))
		// Move frame to front of queue
		MoveToFront(frame_number);
	else
	{
		// Add frame to queue and map
		frames[frame_number] = frame;
		frame_numbers.push_front(frame_number);

		// Increment total bytes (of cache)
		total_bytes += frame->GetBytes();

		// Clean up old frames
		CleanUp();
	}
}

// Check for the existance of a frame in the cache
bool Cache::Exists(int frame_number)
{
	// Is frame number cached
	if (frames.count(frame_number))
		return true;
	else
		return false;
}

// Get a frame from the cache
tr1::shared_ptr<Frame> Cache::GetFrame(int frame_number)
{
	// Does frame exists in cache?
	if (Exists(frame_number))
	{
		// move it to the front of the cache
		MoveToFront(frame_number);

		// return the Frame object
		return frames[frame_number];
	}
	else
		// throw an exception for the missing frame
		throw OutOfBoundsFrame("Frame not found in the cache", frame_number, -1);
}

// Get the smallest frame number
tr1::shared_ptr<Frame> Cache::GetSmallestFrame()
{
	// Loop through frame numbers
	 deque<int>::iterator itr;
	 int smallest_frame = -1;
	 for(itr = frame_numbers.begin(); itr != frame_numbers.end(); ++itr)
	 {
		 if (*itr < smallest_frame || smallest_frame == -1)
			 smallest_frame = *itr;
	 }

	 // Return frame (or error if no frame found)
	 return GetFrame(smallest_frame);
}

// Remove a specific frame
void Cache::Remove(int frame_number)
{
	// Get the frame (or throw exception)
	tr1::shared_ptr<Frame> f = GetFrame(frame_number);

	// Decrement the total bytes (for this cache)
	total_bytes -= f->GetBytes();

	// Loop through frame numbers
	deque<int>::iterator itr;
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
void Cache::MoveToFront(int frame_number)
{
	// Does frame exists in cache?
	if (Exists(frame_number))
	{
		// Loop through frame numbers
		deque<int>::iterator itr;
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
	else
		// throw an exception for the missing frame
		throw OutOfBoundsFrame("Frame not found in the cache", frame_number, -1);
}

// Clear the cache of all frames
void Cache::Clear()
{
	deque<int>::iterator itr;
	for(itr = frame_numbers.begin(); itr != frame_numbers.end(); ++itr)
		// Remove frame from map
		frames.erase(*itr);

	// pop each of the frames from the queue... which empties the queue
	while(!frame_numbers.empty()) frame_numbers.pop_back();

	// Reset total bytes (of cache)
	total_bytes = 0;
}

// Count the frames in the queue
int Cache::Count()
{
	// Return the number of frames in the cache
	return frames.size();
}

// Clean up cached frames that exceed the number in our max_bytes variable
void Cache::CleanUp()
{
	// Do we auto clean up?
	if (max_bytes > 0)
	{
		// check against max bytes (and always leave at least 1 frame in the cache)
		while (total_bytes > max_bytes && frame_numbers.size() > 1)
		{
			// Remove the oldest frame
			int frame_to_remove = frame_numbers.back();

			// Remove frame_number and frame
			Remove(frame_to_remove);
		}
	}
}


// Display a list of cached frame numbers
void Cache::Display()
{
	cout << "----- Cache List (" << frames.size() << ") ------" << endl;
	deque<int>::iterator itr;

	int i = 1;
	for(itr = frame_numbers.begin(); itr != frame_numbers.end(); ++itr)
	{
		cout << " " << i << ") --- Frame " << *itr << endl;
		i++;
	}
}




