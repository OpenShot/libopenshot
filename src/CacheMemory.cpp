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

#include "../include/CacheMemory.h"

using namespace std;
using namespace openshot;

// Default constructor, no max bytes
CacheMemory::CacheMemory() : CacheBase(0) {
	// Set cache type name
	cache_type = "CacheMemory";
	range_version = 0;
	needs_range_processing = false;
};

// Constructor that sets the max bytes to cache
CacheMemory::CacheMemory(long long int max_bytes) : CacheBase(max_bytes) {
	// Set cache type name
	cache_type = "CacheMemory";
	range_version = 0;
	needs_range_processing = false;
};

// Default destructor
CacheMemory::~CacheMemory()
{
	frames.clear();
	frame_numbers.clear();
	ordered_frame_numbers.clear();

	// remove critical section
	delete cacheCriticalSection;
	cacheCriticalSection = NULL;
}


// Calculate ranges of frames
void CacheMemory::CalculateRanges() {
	// Only calculate when something has changed
	if (needs_range_processing) {

		// Create a scoped lock, to protect the cache from multiple threads
		const GenericScopedLock<CriticalSection> lock(*cacheCriticalSection);

		// Sort ordered frame #s, and calculate JSON ranges
		std::sort(ordered_frame_numbers.begin(), ordered_frame_numbers.end());

		// Clear existing JSON variable
		Json::Value ranges = Json::Value(Json::arrayValue);

		// Increment range version
		range_version++;

		vector<long int>::iterator itr_ordered;
		long int starting_frame = *ordered_frame_numbers.begin();
		long int ending_frame = *ordered_frame_numbers.begin();

		// Loop through all known frames (in sequential order)
		for (itr_ordered = ordered_frame_numbers.begin(); itr_ordered != ordered_frame_numbers.end(); ++itr_ordered) {
			long int frame_number = *itr_ordered;
			if (frame_number - ending_frame > 1) {
				// End of range detected
				Json::Value range;

				// Add JSON object with start/end attributes
				// Use strings, since long ints are supported in JSON
				stringstream start_str;
				start_str << starting_frame;
				stringstream end_str;
				end_str << ending_frame;
				range["start"] = start_str.str();
				range["end"] = end_str.str();
				ranges.append(range);

				// Set new starting range
				starting_frame = frame_number;
			}

			// Set current frame as end of range, and keep looping
			ending_frame = frame_number;
		}

		// APPEND FINAL VALUE
		Json::Value range;

		// Add JSON object with start/end attributes
		// Use strings, since long ints are not supported in JSON
		stringstream start_str;
		start_str << starting_frame;
		stringstream end_str;
		end_str << ending_frame;
		range["start"] = start_str.str();
		range["end"] = end_str.str();
		ranges.append(range);

		// Cache range JSON as string
		json_ranges = ranges.toStyledString();

		// Reset needs_range_processing
		needs_range_processing = false;
	}
}

// Add a Frame to the cache
void CacheMemory::Add(std::shared_ptr<Frame> frame)
{
	// Create a scoped lock, to protect the cache from multiple threads
	const GenericScopedLock<CriticalSection> lock(*cacheCriticalSection);
	long int frame_number = frame->number;

	// Freshen frame if it already exists
	if (frames.count(frame_number))
		// Move frame to front of queue
		MoveToFront(frame_number);

	else
	{
		// Add frame to queue and map
		frames[frame_number] = frame;
		frame_numbers.push_front(frame_number);
		ordered_frame_numbers.push_back(frame_number);
		needs_range_processing = true;

		// Clean up old frames
		CleanUp();
	}
}

// Get a frame from the cache (or NULL shared_ptr if no frame is found)
std::shared_ptr<Frame> CacheMemory::GetFrame(long int frame_number)
{
	// Create a scoped lock, to protect the cache from multiple threads
	const GenericScopedLock<CriticalSection> lock(*cacheCriticalSection);

	// Does frame exists in cache?
	if (frames.count(frame_number))
		// return the Frame object
		return frames[frame_number];

	else
		// no Frame found
		return std::shared_ptr<Frame>();
}

// Get the smallest frame number (or NULL shared_ptr if no frame is found)
std::shared_ptr<Frame> CacheMemory::GetSmallestFrame()
{
	// Create a scoped lock, to protect the cache from multiple threads
	const GenericScopedLock<CriticalSection> lock(*cacheCriticalSection);
	std::shared_ptr<openshot::Frame> f;

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
long long int CacheMemory::GetBytes()
{
	// Create a scoped lock, to protect the cache from multiple threads
	const GenericScopedLock<CriticalSection> lock(*cacheCriticalSection);

	long long int total_bytes = 0;

	// Loop through frames, and calculate total bytes
	deque<long int>::reverse_iterator itr;
	for(itr = frame_numbers.rbegin(); itr != frame_numbers.rend(); ++itr)
	{
		total_bytes += frames[*itr]->GetBytes();
	}

	return total_bytes;
}

// Remove a specific frame
void CacheMemory::Remove(long int frame_number)
{
	Remove(frame_number, frame_number);
}

// Remove range of frames
void CacheMemory::Remove(long int start_frame_number, long int end_frame_number)
{
	// Create a scoped lock, to protect the cache from multiple threads
	const GenericScopedLock<CriticalSection> lock(*cacheCriticalSection);

	// Loop through frame numbers
	deque<long int>::iterator itr;
	for(itr = frame_numbers.begin(); itr != frame_numbers.end();)
	{
		if (*itr >= start_frame_number && *itr <= end_frame_number)
		{
			// erase frame number
			itr = frame_numbers.erase(itr);
		}else
			itr++;
	}

	// Loop through ordered frame numbers
	vector<long int>::iterator itr_ordered;
	for(itr_ordered = ordered_frame_numbers.begin(); itr_ordered != ordered_frame_numbers.end();)
	{
		if (*itr_ordered >= start_frame_number && *itr_ordered <= end_frame_number)
		{
			// erase frame number
			frames.erase(*itr_ordered);
			itr_ordered = ordered_frame_numbers.erase(itr_ordered);
		}else
			itr_ordered++;
	}

	// Needs range processing (since cache has changed)
	needs_range_processing = true;
}

// Move frame to front of queue (so it lasts longer)
void CacheMemory::MoveToFront(long int frame_number)
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
void CacheMemory::Clear()
{
	// Create a scoped lock, to protect the cache from multiple threads
	const GenericScopedLock<CriticalSection> lock(*cacheCriticalSection);

	frames.clear();
	frame_numbers.clear();
	ordered_frame_numbers.clear();
	needs_range_processing = true;
}

// Count the frames in the queue
long int CacheMemory::Count()
{
	// Create a scoped lock, to protect the cache from multiple threads
	const GenericScopedLock<CriticalSection> lock(*cacheCriticalSection);

	// Return the number of frames in the cache
	return frames.size();
}

// Clean up cached frames that exceed the number in our max_bytes variable
void CacheMemory::CleanUp()
{
	// Do we auto clean up?
	if (max_bytes > 0)
	{
		// Create a scoped lock, to protect the cache from multiple threads
		const GenericScopedLock<CriticalSection> lock(*cacheCriticalSection);

		while (GetBytes() > max_bytes && frame_numbers.size() > 20)
		{
			// Get the oldest frame number.
			long int frame_to_remove = frame_numbers.back();

			// Remove frame_number and frame
			Remove(frame_to_remove);
		}
	}
}


// Generate JSON string of this object
string CacheMemory::Json() {

	// Return formatted string
	return JsonValue().toStyledString();
}

// Generate Json::JsonValue for this object
Json::Value CacheMemory::JsonValue() {

	// Proccess range data (if anything has changed)
	CalculateRanges();

	// Create root json object
	Json::Value root = CacheBase::JsonValue(); // get parent properties
	root["type"] = cache_type;

	stringstream range_version_str;
	range_version_str << range_version;
	root["version"] = range_version_str.str();

	// Parse and append range data (if any)
	Json::Value ranges;
	Json::Reader reader;
	bool success = reader.parse( json_ranges, ranges );
	if (success)
		root["ranges"] = ranges;

	// return JsonValue
	return root;
}

// Load JSON string into this object
void CacheMemory::SetJson(string value) throw(InvalidJSON) {

	// Parse JSON string into JSON objects
	Json::Value root;
	Json::Reader reader;
	bool success = reader.parse( value, root );
	if (!success)
		// Raise exception
		throw InvalidJSON("JSON could not be parsed (or is invalid)", "");

	try
	{
		// Set all values that match
		SetJsonValue(root);
	}
	catch (exception e)
	{
		// Error parsing JSON (or missing keys)
		throw InvalidJSON("JSON is invalid (missing keys or invalid data types)", "");
	}
}

// Load Json::JsonValue into this object
void CacheMemory::SetJsonValue(Json::Value root) throw(InvalidFile, ReaderClosed) {

	// Close timeline before we do anything (this also removes all open and closing clips)
	Clear();

	// Set parent data
	CacheBase::SetJsonValue(root);

	if (!root["type"].isNull())
		cache_type = root["type"].asString();
}