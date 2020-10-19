/**
 * @file
 * @brief Source file for DummyReader class
 * @author Jonathan Thomas <jonathan@openshot.org>
 *
 * @ref License
 */

/* LICENSE
 *
 * Copyright (c) 2008-2019 OpenShot Studios, LLC
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

#include "DummyReader.h"

using namespace openshot;

// Initialize variables used by constructor
void DummyReader::init(Fraction fps, int width, int height, int sample_rate, int channels, float duration) {
	// Set key info settings
	info.has_audio = false;
	info.has_video = true;
	info.file_size = width * height * sizeof(int);
	info.vcodec = "raw";
	info.fps = fps;
	info.width = width;
	info.height = height;
	info.sample_rate = sample_rate;
	info.channels = channels;
	info.duration = duration;
	info.video_length = duration * fps.ToFloat();
	info.pixel_ratio.num = 1;
	info.pixel_ratio.den = 1;
	info.video_timebase = fps.Reciprocal();
	info.acodec = "raw";

	// Calculate the DAR (display aspect ratio)
	Fraction size(info.width * info.pixel_ratio.num, info.height * info.pixel_ratio.den);

	// Reduce size fraction
	size.Reduce();

	// Set the ratio based on the reduced fraction
	info.display_ratio.num = size.num;
	info.display_ratio.den = size.den;
}

// Blank constructor for DummyReader, with default settings.
DummyReader::DummyReader() : dummy_cache(NULL), is_open(false) {

	// Initialize important variables
	init(Fraction(24,1), 1280, 768, 44100, 2, 30.0);
}

// Constructor for DummyReader.  Pass a framerate and samplerate.
DummyReader::DummyReader(Fraction fps, int width, int height, int sample_rate, int channels, float duration) : dummy_cache(NULL), is_open(false) {

	// Initialize important variables
	init(fps, width, height, sample_rate, channels, duration);
}

// Constructor which also takes a cache object
DummyReader::DummyReader(Fraction fps, int width, int height, int sample_rate, int channels, float duration, CacheBase* cache) : is_open(false) {

	// Initialize important variables
	init(fps, width, height, sample_rate, channels, duration);

	// Set cache object
	dummy_cache = (CacheBase*) cache;
}

DummyReader::~DummyReader() {
}

// Open image file
void DummyReader::Open()
{
	// Open reader if not already open
	if (!is_open)
	{
		// Create or get frame object
		image_frame = std::make_shared<Frame>(1, info.width, info.height, "#000000", info.sample_rate, info.channels);

		// Mark as "open"
		is_open = true;
	}
}

// Close image file
void DummyReader::Close()
{
	// Close all objects, if reader is 'open'
	if (is_open)
	{
		// Mark as "closed"
		is_open = false;
	}
}

// Get an openshot::Frame object for a specific frame number of this reader. It is either a blank frame
// or a custom frame added with passing a Cache object to the constructor.
std::shared_ptr<Frame> DummyReader::GetFrame(int64_t requested_frame)
{
	// Check for open reader (or throw exception)
	if (!is_open)
		throw ReaderClosed("The ImageReader is closed.  Call Open() before calling this method.", "dummy");

	int dummy_cache_count = 0;
	if (dummy_cache) {
		dummy_cache_count = dummy_cache->Count();
	}

	if (dummy_cache_count == 0 && image_frame) {
		// Create a scoped lock, allowing only a single thread to run the following code at one time
		const GenericScopedLock<CriticalSection> lock(getFrameCriticalSection);

		// Always return same frame (regardless of which frame number was requested)
		image_frame->number = requested_frame;
		return image_frame;

	} else if (dummy_cache_count > 0) {
		// Create a scoped lock, allowing only a single thread to run the following code at one time
		const GenericScopedLock<CriticalSection> lock(getFrameCriticalSection);

		// Get a frame from the dummy cache
		std::shared_ptr<openshot::Frame> f = dummy_cache->GetFrame(requested_frame);
		if (f) {
			// return frame from cache (if found)
			return f;
		} else {
			// No cached frame found
			throw InvalidFile("Requested frame not found. You can only access Frame numbers that exist in the Cache object.", "dummy");
		}
	}
	else
		// no frame loaded
		throw InvalidFile("No frame could be created from this type of file.", "dummy");
}

// Generate JSON string of this object
std::string DummyReader::Json() const {

	// Return formatted string
	return JsonValue().toStyledString();
}

// Generate Json::Value for this object
Json::Value DummyReader::JsonValue() const {

	// Create root json object
	Json::Value root = ReaderBase::JsonValue(); // get parent properties
	root["type"] = "DummyReader";

	// return JsonValue
	return root;
}

// Load JSON string into this object
void DummyReader::SetJson(const std::string value) {

	// Parse JSON string into JSON objects
	try
	{
		const Json::Value root = openshot::stringToJson(value);
		// Set all values that match
		SetJsonValue(root);
	}
	catch (const std::exception& e)
	{
		// Error parsing JSON (or missing keys)
		throw InvalidJSON("JSON is invalid (missing keys or invalid data types)");
	}
}

// Load Json::Value into this object
void DummyReader::SetJsonValue(const Json::Value root) {

	// Set parent data
	ReaderBase::SetJsonValue(root);

}
