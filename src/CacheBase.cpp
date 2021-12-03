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

#include <sstream>

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

// Generate Json::Value for this object
Json::Value CacheBase::JsonValue() {

	// Create root json object
	Json::Value root;
	std::stringstream max_bytes_stream;
	max_bytes_stream << max_bytes;
	root["max_bytes"] = max_bytes_stream.str();

	// return JsonValue
	return root;
}

// Load Json::Value into this object
void CacheBase::SetJsonValue(const Json::Value root) {

	// Set data from Json (if key is found)
	if (!root["max_bytes"].isNull())
		max_bytes = std::stoll(root["max_bytes"].asString());
}
