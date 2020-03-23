/**
 * @file
 * @brief Source file for QtImageReader class
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

#include "../include/QtImageReader.h"
#include "../include/Settings.h"
#include "../include/Clip.h"
#include "../include/CacheMemory.h"
#include <QtCore/QString>
#include <QtGui/QImage>
#include <QtGui/QPainter>

#if USE_RESVG == 1
	// If defined and found in CMake, utilize the libresvg for parsing
	// SVG files and rasterizing them to QImages.
	#include "ResvgQt.h"
#endif

using namespace openshot;

QtImageReader::QtImageReader(std::string path) : path{QString::fromStdString(path)}, is_open(false)
{
	// Open and Close the reader, to populate its attributes (such as height, width, etc...)
	Open();
	Close();
}

QtImageReader::QtImageReader(std::string path, bool inspect_reader) : path{QString::fromStdString(path)}, is_open(false)
{
	// Open and Close the reader, to populate its attributes (such as height, width, etc...)
	if (inspect_reader) {
		Open();
		Close();
	}
}

QtImageReader::~QtImageReader()
{
}

// Open image file
void QtImageReader::Open()
{
	// Open reader if not already open
	if (!is_open)
	{
		bool success = true;
		bool loaded = false;

#if USE_RESVG == 1
		// If defined and found in CMake, utilize the libresvg for parsing
		// SVG files and rasterizing them to QImages.
		// Only use resvg for files ending in '.svg' or '.svgz'
		if (path.toLower().endsWith(".svg") || path.toLower().endsWith(".svgz")) {

			ResvgRenderer renderer(path);
			if (renderer.isValid()) {

				image = std::shared_ptr<QImage>(new QImage(renderer.defaultSize(), QImage::Format_ARGB32_Premultiplied));
				image->fill(Qt::transparent);

				QPainter p(image.get());
				renderer.render(&p);
				p.end();
				loaded = true;
			}
		}
#endif

		if (!loaded) {
			// Attempt to open file using Qt's build in image processing capabilities
			image = std::shared_ptr<QImage>(new QImage());
			success = image->load(path);
		}

		if (!success) {
			// raise exception
			throw InvalidFile("File could not be opened.", path.toStdString());
		}

		// Convert to proper format
		image = std::shared_ptr<QImage>(new QImage(image->convertToFormat(QImage::Format_RGBA8888)));

		// Update image properties
		info.has_audio = false;
		info.has_video = true;
		info.has_single_image = true;
		#if QT_VERSION >= QT_VERSION_CHECK(5, 10, 0)
			// byteCount() is deprecated from Qt 5.10
			info.file_size = image->sizeInBytes();
		#else
			info.file_size = image->byteCount();
		#endif
		info.vcodec = "QImage";
		info.width = image->width();
		info.height = image->height();
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

		// Set current max size
		max_size.setWidth(info.width);
		max_size.setHeight(info.height);

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
std::shared_ptr<Frame> QtImageReader::GetFrame(int64_t requested_frame)
{
	// Check for open reader (or throw exception)
	if (!is_open)
		throw ReaderClosed("The Image is closed.  Call Open() before calling this method.", path.toStdString());

	// Create a scoped lock, allowing only a single thread to run the following code at one time
	const GenericScopedLock<CriticalSection> lock(getFrameCriticalSection);

	// Determine the max size of this source image (based on the timeline's size, the scaling mode,
	// and the scaling keyframes). This is a performance improvement, to keep the images as small as possible,
	// without losing quality. NOTE: We cannot go smaller than the timeline itself, or the add_layer timeline
	// method will scale it back to timeline size before scaling it smaller again. This needs to be fixed in
	// the future.
	int max_width = Settings::Instance()->MAX_WIDTH;
	if (max_width <= 0)
		max_width = info.width;
	int max_height = Settings::Instance()->MAX_HEIGHT;
	if (max_height <= 0)
		max_height = info.height;

	Clip* parent = (Clip*) GetClip();
	if (parent) {
		if (parent->scale == SCALE_FIT || parent->scale == SCALE_STRETCH) {
			// Best fit or Stretch scaling (based on max timeline size * scaling keyframes)
			float max_scale_x = parent->scale_x.GetMaxPoint().co.Y;
			float max_scale_y = parent->scale_y.GetMaxPoint().co.Y;
			max_width = std::max(float(max_width), max_width * max_scale_x);
			max_height = std::max(float(max_height), max_height * max_scale_y);

		} else if (parent->scale == SCALE_CROP) {
			// Cropping scale mode (based on max timeline size * cropped size * scaling keyframes)
			float max_scale_x = parent->scale_x.GetMaxPoint().co.Y;
			float max_scale_y = parent->scale_y.GetMaxPoint().co.Y;
			QSize width_size(max_width * max_scale_x,
							 round(max_width / (float(info.width) / float(info.height))));
			QSize height_size(round(max_height / (float(info.height) / float(info.width))),
							  max_height * max_scale_y);
			// respect aspect ratio
			if (width_size.width() >= max_width && width_size.height() >= max_height) {
				max_width = std::max(max_width, width_size.width());
				max_height = std::max(max_height, width_size.height());
			}
			else {
				max_width = std::max(max_width, height_size.width());
				max_height = std::max(max_height, height_size.height());
			}

		} else {
			// No scaling, use original image size (slower)
			max_width = info.width;
			max_height = info.height;
		}
	}

	// Scale image smaller (or use a previous scaled image)
	if (!cached_image || (max_size.width() != max_width || max_size.height() != max_height)) {

		bool rendered = false;
#if USE_RESVG == 1
		// If defined and found in CMake, utilize the libresvg for parsing
		// SVG files and rasterizing them to QImages.
		// Only use resvg for files ending in '.svg' or '.svgz'
		if (path.toLower().endsWith(".svg") || path.toLower().endsWith(".svgz")) {

			ResvgRenderer renderer(path);
			if (renderer.isValid()) {
				// Scale SVG size to keep aspect ratio, and fill the max_size as best as possible
				QSize svg_size(renderer.defaultSize().width(), renderer.defaultSize().height());
				svg_size.scale(max_width, max_height, Qt::KeepAspectRatio);

				// Create empty QImage
				cached_image = std::shared_ptr<QImage>(new QImage(QSize(svg_size.width(), svg_size.height()), QImage::Format_ARGB32_Premultiplied));
				cached_image->fill(Qt::transparent);

				// Render SVG into QImage
				QPainter p(cached_image.get());
				renderer.render(&p);
				p.end();
				rendered = true;
			}
		}
#endif

		if (!rendered) {
			// We need to resize the original image to a smaller image (for performance reasons)
			// Only do this once, to prevent tons of unneeded scaling operations
			cached_image = std::shared_ptr<QImage>(new QImage(image->scaled(max_width, max_height, Qt::KeepAspectRatio, Qt::SmoothTransformation)));
		}

		cached_image = std::shared_ptr<QImage>(new QImage(cached_image->convertToFormat(QImage::Format_RGBA8888)));

		// Set max size (to later determine if max_size is changed)
		max_size.setWidth(max_width);
		max_size.setHeight(max_height);
	}

	// Create or get frame object
	std::shared_ptr<Frame> image_frame(new Frame(requested_frame, cached_image->width(), cached_image->height(), "#000000", Frame::GetSamplesPerFrame(requested_frame, info.fps, info.sample_rate, info.channels), info.channels));

	// Add Image data to frame
	image_frame->AddImage(cached_image);

	// return frame object
	return image_frame;
}

// Generate JSON string of this object
std::string QtImageReader::Json() const {

	// Return formatted string
	return JsonValue().toStyledString();
}

// Generate Json::Value for this object
Json::Value QtImageReader::JsonValue() const {

	// Create root json object
	Json::Value root = ReaderBase::JsonValue(); // get parent properties
	root["type"] = "QtImageReader";
	root["path"] = path.toStdString();

	// return JsonValue
	return root;
}

// Load JSON string into this object
void QtImageReader::SetJson(const std::string value) {

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
void QtImageReader::SetJsonValue(const Json::Value root) {

	// Set parent data
	ReaderBase::SetJsonValue(root);

	// Set data from Json (if key is found)
	if (!root["path"].isNull())
		path = QString::fromStdString(root["path"].asString());

	// Re-Open path, and re-init everything (if needed)
	if (is_open)
	{
		Close();
		Open();
	}
}
