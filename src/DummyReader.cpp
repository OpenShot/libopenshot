/**
 * @file
 * @brief Source file for DummyReader class
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

#include "../include/DummyReader.h"

using namespace openshot;

// Blank constructor for DummyReader, with default settings.
DummyReader::DummyReader() {

	// Call actual constructor with default values
	DummyReader(Fraction(24,1), 1280, 768, 44100, 2, 30.0);
}

// Constructor for DummyReader.  Pass a framerate and samplerate.
DummyReader::DummyReader(Fraction fps, int width, int height, int sample_rate, int channels, float duration) {

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

	// Open and Close the reader, to populate it's attributes (such as height, width, etc...)
	Open();
	Close();
}

// Open image file
void DummyReader::Open() throw(InvalidFile)
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

// Get an openshot::Frame object for a specific frame number of this reader.
std::shared_ptr<Frame> DummyReader::GetFrame(long int requested_frame) throw(ReaderClosed)
{
	// Check for open reader (or throw exception)
	if (!is_open)
		throw ReaderClosed("The ImageReader is closed.  Call Open() before calling this method.", "dummy");

	if (image_frame)
	{
		// Create a scoped lock, allowing only a single thread to run the following code at one time
		const GenericScopedLock<CriticalSection> lock(getFrameCriticalSection);

		// Always return same frame (regardless of which frame number was requested)
		image_frame->number = requested_frame;
		return image_frame;
	}
	else
		// no frame loaded
		throw InvalidFile("No frame could be created from this type of file.", "dummy");
}

// Generate JSON string of this object
string DummyReader::Json() {

	// Return formatted string
	return JsonValue().toStyledString();
}

// Generate Json::JsonValue for this object
Json::Value DummyReader::JsonValue() {

	// Create root json object
	Json::Value root = ReaderBase::JsonValue(); // get parent properties
	root["type"] = "DummyReader";

	// return JsonValue
	return root;
}

// Load JSON string into this object
void DummyReader::SetJson(string value) throw(InvalidJSON) {

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
void DummyReader::SetJsonValue(Json::Value root) throw(InvalidFile) {

	// Set parent data
	ReaderBase::SetJsonValue(root);

}
