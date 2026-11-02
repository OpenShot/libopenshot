/**
 * @file
 * @brief Source file for ImageReader class
 * @author Jonathan Thomas <jonathan@openshot.org>
 *
 * @ref License
 */

// Copyright (c) 2008-2019 OpenShot Studios, LLC
//
// SPDX-License-Identifier: LGPL-3.0-or-later

// Require ImageMagick support
#ifdef USE_IMAGEMAGICK

#include "MagickUtilities.h"
#include "QtUtilities.h"

#include "ImageReader.h"
#include "Exceptions.h"
#include "Frame.h"

using namespace openshot;

ImageReader::ImageReader(const std::string& path, bool inspect_reader) : path(path), is_open(false)
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
	const std::lock_guard<std::mutex> lock(cache_mutex);
	OpenUnlocked();
}

void ImageReader::OpenUnlocked()
{
	// Open reader if not already open
	if (!is_open)
	{
		// Attempt to open file
		try
		{
			// load image
			image = std::make_shared<Magick::Image>(path);

			// Give image a transparent background color
			image->backgroundColor(Magick::Color("none"));
			MAGICK_IMAGE_ALPHA(image, true);
		}
		catch (const Magick::Exception& e) {
			// raise exception
			throw InvalidFile("ImageReader could not open image file.", path);
		}

		// Update image properties
		info.has_audio = false;
		info.has_video = true;
		info.has_single_image = true;
		info.file_size = image->fileSize();
		info.vcodec = image->format();
		info.width = image->size().width();
		info.height = image->size().height();
		info.pixel_ratio = openshot::Fraction(1, 1);
		info.fps = openshot::Fraction(30, 1);
		info.video_timebase = info.fps.Reciprocal();
		// Default still-image duration: 1 hour, aligned to fps
		info.video_length = 60 * 60 * info.fps.num; // 3600 seconds * 30 fps
		info.duration = static_cast<float>(info.video_length / info.fps.ToDouble());

		// Calculate the DAR (display aspect ratio)
		Fraction dar(
			info.width * info.pixel_ratio.num,
			info.height * info.pixel_ratio.den);

		// Reduce DAR fraction & set ratio
		dar.Reduce();
		info.display_ratio = dar;

		// Mark as "open"
		is_open = true;
	}
}

void ImageReader::Close()
{
	const std::lock_guard<std::mutex> lock(cache_mutex);
	CloseUnlocked();
}

void ImageReader::CloseUnlocked()
{
	// Always release cached resources, including after partial setup.
	cached_image.reset();
	image.reset();
	is_open = false;
}

bool ImageReader::IsOpen()
{
	const std::lock_guard<std::mutex> lock(cache_mutex);
	return is_open;
}

// Get an openshot::Frame object for a specific frame number of this reader.
std::shared_ptr<Frame> ImageReader::GetFrame(int64_t requested_frame)
{
	const std::lock_guard<std::mutex> lock(cache_mutex);

	if (!is_open) {
		throw ReaderClosed(
			"The ImageReader is closed. "
			"Call Open() before calling this method.", path);
	}

	// Create or get frame object
	auto image_frame = std::make_shared<Frame>(
		requested_frame,
		image->size().width(), image->size().height(),
		"#000000", 0, 2);

	// Convert once per Open() cycle, under the reader mutex.
	if (!cached_image)
		cached_image = openshot::Magick2QImage(image);

	// Use a separate QImage object with implicitly shared pixel data.
	auto qimage = std::make_shared<QImage>(*cached_image);
	image_frame->AddImage(qimage);
	return image_frame;
}

// Generate JSON string of this object
std::string ImageReader::Json() const {

	// Return formatted string
	return JsonValue().toStyledString();
}

// Generate Json::Value for this object
Json::Value ImageReader::JsonValue() const {
	const std::lock_guard<std::mutex> lock(cache_mutex);

	// get parent properties
	Json::Value root = ReaderBase::JsonValue();

	root["type"] = "ImageReader";
	root["path"] = path;
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
        throw InvalidJSON(
            "JSON is invalid (missing keys or invalid data types)");
    }
}

// Load Json::Value into this object
void ImageReader::SetJsonValue(const Json::Value root) {
	const std::lock_guard<std::mutex> lock(cache_mutex);

	// Set parent data
	ReaderBase::SetJsonValue(root);

	// Set data from Json (if key is found)
	if (!root["path"].isNull())
		path = root["path"].asString();

	if (is_open) {
		CloseUnlocked();
		OpenUnlocked();
	}
}

#endif //USE_IMAGEMAGICK
