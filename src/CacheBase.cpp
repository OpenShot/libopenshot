/**
 * @file
 * @brief Source file for CacheBase class
 * @author Jonathan Thomas <jonathan@openshot.org>
 *
 * @ref License
 */

// Copyright (c) 2008-2019 OpenShot Studios, LLC
//
// SPDX-License-Identifier: LGPL-3.0-or-later

#include "CacheBase.h"

using namespace std;
using namespace openshot;

// Default constructor, no max frames
CacheBase::CacheBase() : CacheBase::CacheBase(0) { }

// Constructor that sets the max frames to cache
CacheBase::CacheBase(int64_t max_bytes) : max_bytes(max_bytes) {
	// Init the mutex
	cacheMutex = new std::recursive_mutex();
}

// Set maximum bytes to a different amount based on a ReaderInfo struct
void CacheBase::SetMaxBytesFromInfo(int64_t number_of_frames, int width, int height, int sample_rate, int channels)
{
	// n frames X height X width X 4 colors of chars X audio channels X 4 byte floats
	int64_t bytes = number_of_frames * (height * width * 4 + (sample_rate * channels * 4));
	SetMaxBytes(bytes);
}

// Calculate ranges of frames
void CacheBase::CalculateRanges() {
	// Only calculate when something has changed
	if (needs_range_processing) {

		// Create a scoped lock, to protect the cache from multiple threads
		const std::lock_guard<std::recursive_mutex> lock(*cacheMutex);

		// Sort ordered frame #s, and calculate JSON ranges
		std::sort(ordered_frame_numbers.begin(), ordered_frame_numbers.end());

		// Clear existing JSON variable
		Json::Value ranges = Json::Value(Json::arrayValue);

		// Increment range version
		range_version++;

		std::vector<int64_t>::iterator itr_ordered;

        int64_t starting_frame = 0;
        int64_t ending_frame = 0;
        if (ordered_frame_numbers.size() > 0) {
            starting_frame = *ordered_frame_numbers.begin();
            ending_frame = *ordered_frame_numbers.begin();

            // Loop through all known frames (in sequential order)
            for (itr_ordered = ordered_frame_numbers.begin(); itr_ordered != ordered_frame_numbers.end(); ++itr_ordered) {
                int64_t frame_number = *itr_ordered;
                if (frame_number - ending_frame > 1) {
                    // End of range detected
                    Json::Value range;

                    // Add JSON object with start/end attributes
                    // Use strings, since int64_ts are supported in JSON
                    range["start"] = std::to_string(starting_frame);
                    range["end"] = std::to_string(ending_frame);
                    ranges.append(range);

                    // Set new starting range
                    starting_frame = frame_number;
                }

                // Set current frame as end of range, and keep looping
                ending_frame = frame_number;
            }
        }

        // APPEND FINAL VALUE
        Json::Value range;

        // Add JSON object with start/end attributes
        // Use strings, since int64_ts are not supported in JSON
        range["start"] = std::to_string(starting_frame);
        range["end"] = std::to_string(ending_frame);
        ranges.append(range);

        // Cache range JSON as string
        json_ranges = ranges.toStyledString();

        // Reset needs_range_processing
        needs_range_processing = false;
    }
}

// Generate Json::Value for this object
Json::Value CacheBase::JsonValue() {

	// Create root json object
	Json::Value root;
	root["max_bytes"] = std::to_string(max_bytes);

	// return JsonValue
	return root;
}

// Load Json::Value into this object
void CacheBase::SetJsonValue(const Json::Value root) {

	// Set data from Json (if key is found)
	if (!root["max_bytes"].isNull())
		max_bytes = std::stoll(root["max_bytes"].asString());
}
