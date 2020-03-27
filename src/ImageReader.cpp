/**
 * @file
 * @brief Source file for ImageReader class
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

// Require ImageMagick support
#ifdef USE_IMAGEMAGICK

#include "../include/ImageReader.h"

using namespace openshot;

ImageReader::ImageReader(std::string path) : path(path), is_open(false)
{
	// Open and Close the reader, to populate its attributes (such as height, width, etc...)
	Open();
	Close();
}

ImageReader::ImageReader(std::string path, bool inspect_reader) : path(path), is_open(false)
{
	// Open and Close the reader, to populate its attributes (such as height, width, etc...)
	if (inspect_reader) {
		Open();
		Close();
	}
}

// Open image file
void ImageReader::Open()
{
	// Open reader if not already open
	if (!is_open)
	{
		// Attempt to open file
		try
		{
			// load image
			image = std::shared_ptr<Magick::Image>(new Magick::Image(path));

			// Give image a transparent background color
			image->backgroundColor(Magick::Color("none"));
			MAGICK_IMAGE_ALPHA(image, true);
		}
		catch (const Magick::Exception& e) {
			// raise exception
			throw InvalidFile("File could not be opened.", path);
		}

		// Update image properties
		info.has_audio = false;
		info.has_video = true;
		info.has_single_image = true;
		info.file_size = image->fileSize();
		info.vcodec = image->format();
		info.width = image->size().width();
		info.height = image->size().height();
		info.pixel_ratio.num = 1;
		info.pixel_ratio.den = 1;
		info.duration = 60 * 60 * 1;  // 1 hour duration
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
void ImageReader::Close()
{
	// Close all objects, if reader is 'open'
	if (is_open)
	{
		// Mark as "closed"
		is_open = false;

		// Delete the image
		image.reset();
	}
}

// Get an openshot::Frame object for a specific frame number of this reader.
std::shared_ptr<Frame> ImageReader::GetFrame(int64_t requested_frame)
{
	// Check for open reader (or throw exception)
	if (!is_open)
		throw ReaderClosed("The FFmpegReader is closed.  Call Open() before calling this method.", path);

	// Create or get frame object
	std::shared_ptr<Frame> image_frame(new Frame(requested_frame, image->size().width(), image->size().height(), "#000000", 0, 2));

	// Add Image data to frame
	image_frame->AddMagickImage(image);

	// return frame object
	return image_frame;
}

// Generate JSON string of this object
std::string ImageReader::Json() const {

	// Return formatted string
	return JsonValue().toStyledString();
}

// Generate Json::Value for this object
Json::Value ImageReader::JsonValue() const {

	// Create root json object
	Json::Value root = ReaderBase::JsonValue(); // get parent properties
	root["type"] = "ImageReader";
	root["path"] = path;

	// return JsonValue
	return root;
}

// Load JSON string into this object
void ImageReader::SetJson(const std::string value) {

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
void ImageReader::SetJsonValue(const Json::Value root) {

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

#endif //USE_IMAGEMAGICK
