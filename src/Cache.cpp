/**
 * \file
 * \brief Source code for the Cache class
 * \author Copyright (c) 2011 Jonathan Thomas
 */

#include "../include/Cache.h"

using namespace std;
using namespace openshot;

// Default constructor, max frames to cache is 20
Cache::Cache() : max_frames(20) { };

// Constructor that sets the max frames to cache
Cache::Cache(int max_frames) : max_frames(max_frames) { };

// Add a Frame to the cache
void Cache::Add(int frame_number, Frame frame)
{
	// Only add frame if it does not exist in the cache
	if (!frames.count(frame_number))
	{
		// Add frame to queue and map
		frames[frame_number] = frame;
		frame_numbers.push_front(frame_number);

		// Clean up older frames (that exceed the max frames)
		CleanUp();
	}
	else
	{
		// Frame already exists... so just update the cache
		frames.erase(frame_number);
		frames[frame_number] = frame;
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
Frame Cache::GetFrame(int frame_number)
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
Frame Cache::GetSmallestFrame()
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
	Frame f = GetFrame(frame_number);

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

// Clear the cache of all frames
void Cache::Clear()
{
	// clear map
	frames.clear();

	// pop each of the frames from the queue... which emptys the queue
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
	// check against max size
	if (frame_numbers.size() > max_frames)
	{
		// Remove the oldest frame
		int frame_to_remove = frame_numbers.back();

		// Remove frame_number and frame
		Remove(frame_to_remove);
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




