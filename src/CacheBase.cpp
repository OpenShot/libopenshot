/**
 * @file
 * @brief Source file for CacheBase class
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

#include "../include/CacheBase.h"

using namespace std;
using namespace openshot;

// Default constructor, no max frames
CacheBase::CacheBase() : max_bytes(0) {
	// Init the critical section
	cacheCriticalSection = new CriticalSection();
};

// Constructor that sets the max frames to cache
CacheBase::CacheBase(long long int max_bytes) : max_bytes(max_bytes) {
	// Init the critical section
	cacheCriticalSection = new CriticalSection();
};

// Set maximum bytes to a different amount based on a ReaderInfo struct
void CacheBase::SetMaxBytesFromInfo(long int number_of_frames, int width, int height, int sample_rate, int channels)
{
	// n frames X height X width X 4 colors of chars X audio channels X 4 byte floats
	long long int bytes = number_of_frames * (height * width * 4 + (sample_rate * channels * 4));
	SetMaxBytes(bytes);
}

// Generate Json::JsonValue for this object
Json::Value CacheBase::JsonValue() {

	// Create root json object
	Json::Value root;
	stringstream max_bytes_stream;
	max_bytes_stream << max_bytes;
	root["max_bytes"] = max_bytes_stream.str();

	// return JsonValue
	return root;
}

// Load Json::JsonValue into this object
void CacheBase::SetJsonValue(Json::Value root) {

	// Set data from Json (if key is found)
	if (!root["max_bytes"].isNull())
		max_bytes = atoll(root["max_bytes"].asString().c_str());
}