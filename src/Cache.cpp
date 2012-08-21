/**
 * \file
 * \brief Source code for the Cache class
 * \author Copyright (c) 2011 Jonathan Thomas
 */

#include "../include/Cache.h"

using namespace std;
using namespace openshot;

// Default constructor, max frames to cache is 20
Cache::Cache() : max_frames(20), current_frame(0) { };

// Constructor that sets the max frames to cache
Cache::Cache(int max_frames) : max_frames(max_frames), current_frame(0) { };

// Add a Frame to the cache
void Cache::Add(int frame_number, Frame *frame)
{
	// Only add frame if it does not exist in the cache
	if (!frames.count(frame_number))
	{
		// Add frame to queue and map
		frames[frame_number] = frame;
		frame_numbers.push_front(frame_number);
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
Frame* Cache::GetFrame(int frame_number)
{
	// Does frame exists in cache?
	if (Exists(frame_number))
	{
		// return the Frame object
		return frames[frame_number];
	}
	else
		// throw an exception for the missing frame
		throw OutOfBoundsFrame("Frame not found in the cache", frame_number, -1);
}

// Get the smallest frame number
Frame* Cache::GetSmallestFrame()
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
void Cache::Remove(int frame_number, bool delete_data)
{
	// Get the frame (or throw exception)
	Frame *f = GetFrame(frame_number);

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

	// Deallocate frame (if requested)
	if (delete_data)
		delete frames[frame_number];

	// Remove frame from map
	frames.erase(frame_number);
}

// Remove a specific frame
void Cache::Remove(int frame_number)
{
	// Remove and delete frame data
	Remove(frame_number, true);
}

// Clear the cache of all frames
void Cache::Clear()
{
	deque<int>::iterator itr;
	for(itr = frame_numbers.begin(); itr != frame_numbers.end(); ++itr)
	{
		// Deallocate frame
		delete frames[*itr];

		// Remove frame from map
		frames.erase(*itr);
	}

	// pop each of the frames from the queue... which empties the queue
	while(!frame_numbers.empty()) frame_numbers.pop_back();
}

// Count the frames in the queue
int Cache::Count()
{
	// Return the number of frames in the cache
	return frames.size();
}

// Clean up cached frames that exceed the number in our max_frames variable
void Cache::CleanUp()
{
	// Count previous frames (relative to the current frame), and determine the smallest frame number
	// Loop through frame numbers
	deque<int>::iterator itr;
	int previous_frames = 0;
	int smallest_frame = -1;
	for (itr = frame_numbers.begin(); itr != frame_numbers.end(); ++itr) {
		if (*itr < current_frame)
			previous_frames++;

		if (*itr < smallest_frame || smallest_frame == -1)
			smallest_frame = *itr;
	}

	// check against max size
	if (previous_frames > max_frames) {
		// Get the difference
		int diff = previous_frames - max_frames;
		int removed_count = 0;

		// Loop through frames and remove the oldest
		for (int x = smallest_frame; x < current_frame; x++) {

			// Does frame exist?
			if (Exists(x)) {
				// Remove the frame, increment count
				Remove(x);
				removed_count++;
			}

			// Break after the correct # has been removed
			if (removed_count == diff)
				break;
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

// Display the list of cache and clear the cache (mainly for debugging reasons)
void Cache::DisplayAndClear()
{
	cout << "----- Cache List (" << frames.size() << ") ------" << endl;
	int i = 1;
	while(!frame_numbers.empty())
	{
		// Print the frame number
		int frame_number = frame_numbers.back();
		cout << " " << i << ") --- Frame " << frame_number << endl;

		// Remove this frame
		Remove(frame_number);

		// increment counter
		i++;
	}
}



