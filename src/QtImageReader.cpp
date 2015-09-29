/**
 * @file
 * @brief Source file for QtImageReader class
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

#include "../include/QtImageReader.h"

using namespace openshot;

QtImageReader::QtImageReader(string path) throw(InvalidFile) : path(path), is_open(false)
{
	// Open and Close the reader, to populate it's attributes (such as height, width, etc...)
	Open();
	Close();
}

// Open image file
void QtImageReader::Open() throw(InvalidFile)
{
	// Open reader if not already open
	if (!is_open)
	{
		// Attempt to open file
		image = tr1::shared_ptr<QImage>(new QImage());
		bool success = image->load(QString::fromStdString(path));

		// Set pixel format
		image = tr1::shared_ptr<QImage>(new QImage(image->convertToFormat(QImage::Format_RGBA8888)));

		if (!success)
			// raise exception
			throw InvalidFile("File could not be opened.", path);

		// Update image properties
		info.has_audio = false;
		info.has_video = true;
		info.has_single_image = true;
		info.file_size = image->byteCount();
		info.vcodec = "QImage";
		info.width = image->width();
		info.height = image->height();
		info.pixel_ratio.num = 1;
		info.pixel_ratio.den = 1;
		info.duration = 60 * 60 * 24; // 24 hour duration
		info.fps.num = 30;
		info.fps.den = 1;
		info.video_timebase.num = 1;
		info.video_timebase.den = 30;
		info.video_length = round(info.duration * info.fps.ToDouble());

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
void QtImageReader::Close()
{
	// Close all objects, if reader is 'open'
	if (is_open)
	{
		// Mark as "closed"
		is_open = false;
		
		// Delete the image
		image.reset();

		info.vcodec = "";
		info.acodec = "";
	}
}

// Get an openshot::Frame object for a specific frame number of this reader.
tr1::shared_ptr<Frame> QtImageReader::GetFrame(long int requested_frame) throw(ReaderClosed)
{
	// Check for open reader (or throw exception)
	if (!is_open)
		throw ReaderClosed("The Image is closed.  Call Open() before calling this method.", path);

	// Create or get frame object
	tr1::shared_ptr<Frame> image_frame(new Frame(requested_frame, info.width, info.height, "#000000", Frame::GetSamplesPerFrame(requested_frame, info.fps, info.sample_rate, info.channels), info.channels));

	// Add Image data to frame
	image_frame->AddImage(image);

	// return frame object
	return image_frame;
}

// Generate JSON string of this object
string QtImageReader::Json() {

	// Return formatted string
	return JsonValue().toStyledString();
}

// Generate Json::JsonValue for this object
Json::Value QtImageReader::JsonValue() {

	// Create root json object
	Json::Value root = ReaderBase::JsonValue(); // get parent properties
	root["type"] = "QtImageReader";
	root["path"] = path;

	// return JsonValue
	return root;
}

// Load JSON string into this object
void QtImageReader::SetJson(string value) throw(InvalidJSON) {

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
void QtImageReader::SetJsonValue(Json::Value root) throw(InvalidFile) {

	// Set parent data
	ReaderBase::SetJsonValue(root);

	// Set data from Json (if key is found)
	if (!root["path"].isNull())
		path = root["path"].asString();

	// Re-Open path, and re-init everything (if needed)
	if (is_open)
	{
		Close();
		Open();
	}
}
