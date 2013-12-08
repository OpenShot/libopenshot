/**
 * @file
 * @brief Source file for DummyReader class
 * @author Jonathan Thomas <jonathan@openshot.org>
 *
 * @section LICENSE
 *
 * Copyright (c) 2008-2013 OpenShot Studios, LLC
 * (http://www.openshotstudios.com). This file is part of
 * OpenShot Library (http://www.openshot.org), an open-source project
 * dedicated to delivering high quality video editing and animation solutions
 * to the world.
 *
 * OpenShot Library is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * OpenShot Library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with OpenShot Library.  If not, see <http://www.gnu.org/licenses/>.
 */

#include "../include/DummyReader.h"

using namespace openshot;

// Constructor for DummyReader.  Pass a framerate and samplerate.
DummyReader::DummyReader(Framerate fps, int width, int height, int sample_rate, int channels, float duration) :
		fps(fps), width(width), height(height), sample_rate(sample_rate), channels(channels), duration(duration)
{
	// Init FileInfo struct (clear all values)
	InitFileInfo();

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
		image_frame = tr1::shared_ptr<Frame>(new Frame(1, width, height, "#000000", sample_rate, channels));

		// Add Image data to frame
		image_frame->AddImage(tr1::shared_ptr<Magick::Image>(new Magick::Image(Magick::Geometry(width, height), Magick::Color("#000000"))));

		// Update image properties
		info.has_audio = false;
		info.has_video = true;
		info.file_size = width * height * sizeof(int);
		info.vcodec = "raw";
		info.width = width;
		info.height = height;
		info.pixel_ratio.num = 1;
		info.pixel_ratio.den = 1;
		info.duration = duration;
		info.fps.num = fps.GetFraction().num;
		info.fps.den = fps.GetFraction().den;
		info.video_timebase.num = fps.GetFraction().den;
		info.video_timebase.den = fps.GetFraction().num;
		info.video_length = round(info.duration * info.fps.ToDouble());
		info.acodec = "raw";
		info.channels = channels;
		info.sample_rate = sample_rate;

		// Calculate the DAR (display aspect ratio)
		Fraction size(info.width * info.pixel_ratio.num, info.height * info.pixel_ratio.den);

		// Reduce size fraction
		size.Reduce();

		// Set the ratio based on the reduced fraction
		info.display_ratio.num = size.num;
		info.display_ratio.den = size.den;

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
tr1::shared_ptr<Frame> DummyReader::GetFrame(int requested_frame) throw(ReaderClosed)
{
	// Check for open reader (or throw exception)
	if (!is_open)
		throw ReaderClosed("The ImageReader is closed.  Call Open() before calling this method.", "dummy");

	if (image_frame)
	{
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
