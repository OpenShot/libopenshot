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

#include "QtImageReader.h"
#include "Exceptions.h"
#include "Settings.h"
#include "Clip.h"
#include "CacheMemory.h"
#include "Timeline.h"
#include <QtCore/QString>
#include <QtGui/QImage>
#include <QtGui/QPainter>
#include <QtGui/QIcon>
#include <QtGui/QImageReader>

#if USE_RESVG == 1
	// If defined and found in CMake, utilize the libresvg for parsing
	// SVG files and rasterizing them to QImages.
	#include "ResvgQt.h"
#endif

using namespace openshot;

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
		bool loaded = false;

        // Check for SVG files and rasterizing them to QImages
        if (path.toLower().endsWith(".svg") || path.toLower().endsWith(".svgz")) {
            loaded = load_svg_path(path);
        }

		if (!loaded) {
			// Attempt to open file using Qt's build in image processing capabilities
			// AutoTransform enables exif data to be parsed and auto transform the image
			// to the correct orientation
			image = std::make_shared<QImage>();
            QImageReader imgReader( path );
            imgReader.setAutoTransform( true );
            loaded = imgReader.read(image.get());
		}

		if (!loaded) {
			// raise exception
			throw InvalidFile("File could not be opened.", path.toStdString());
		}

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

    // Calculate max image size
    QSize current_max_size = calculate_max_size();

	// Scale image smaller (or use a previous scaled image)
	if (!cached_image || (max_size.width() != current_max_size.width() || max_size.height() != current_max_size.height())) {
        // Check for SVG files and rasterize them to QImages
        if (path.toLower().endsWith(".svg") || path.toLower().endsWith(".svgz")) {
            load_svg_path(path);
        }

        // We need to resize the original image to a smaller image (for performance reasons)
        // Only do this once, to prevent tons of unneeded scaling operations
        cached_image = std::make_shared<QImage>(image->scaled(
                current_max_size.width(), current_max_size.height(), Qt::KeepAspectRatio, Qt::SmoothTransformation));

		// Set max size (to later determine if max_size is changed)
		max_size.setWidth(current_max_size.width());
		max_size.setHeight(current_max_size.height());
	}

	// Create or get frame object
	auto image_frame = std::make_shared<Frame>(
		requested_frame, cached_image->width(), cached_image->height(), "#000000",
		Frame::GetSamplesPerFrame(requested_frame, info.fps, info.sample_rate, info.channels),
		info.channels);

	// Add Image data to frame
	image_frame->AddImage(cached_image);

	// return frame object
	return image_frame;
}

// Calculate the max_size QSize, based on parent timeline and parent clip settings
QSize QtImageReader::calculate_max_size() {
    // Get max project size
    int max_width = info.width;
    int max_height = info.height;
    if (max_width == 0 || max_height == 0) {
        // If no size determined yet, default to 4K
        max_width = 1920;
        max_height = 1080;
    }

    Clip* parent = (Clip*) ParentClip();
    if (parent) {
        if (parent->ParentTimeline()) {
            // Set max width/height based on parent clip's timeline (if attached to a timeline)
            max_width = parent->ParentTimeline()->preview_width;
            max_height = parent->ParentTimeline()->preview_height;
        }
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
        }
    }

    // Return new QSize of the current max size
    return QSize(max_width, max_height);
}

// Load an SVG file with Resvg or fallback with Qt
bool QtImageReader::load_svg_path(QString) {
    bool loaded = false;

    // Calculate max image size
    QSize current_max_size = calculate_max_size();

#if USE_RESVG == 1
    // Use libresvg for parsing/rasterizing SVG
    ResvgRenderer renderer(path);
    if (renderer.isValid()) {
        // Scale SVG size to keep aspect ratio, and fill the max_size as best as possible
        QSize svg_size(renderer.defaultSize().width(), renderer.defaultSize().height());
        svg_size.scale(current_max_size.width(), current_max_size.height(), Qt::KeepAspectRatio);

        // Load SVG at max size
        image = std::make_shared<QImage>(svg_size, QImage::Format_RGBA8888_Premultiplied);
        image->fill(Qt::transparent);
        QPainter p(image.get());
        renderer.render(&p);
        p.end();
        loaded = true;
    }
#endif

    if (!loaded) {
        // Use Qt for parsing/rasterizing SVG
        image = std::make_shared<QImage>();
        loaded = image->load(path);

        if (loaded && (image->width() < current_max_size.width() || image->height() < current_max_size.height())) {
            // Load SVG into larger/project size (so image is not blurry)
            QSize svg_size = image->size().scaled(current_max_size.width(), current_max_size.height(), Qt::KeepAspectRatio);
            if (QCoreApplication::instance()) {
                // Requires QApplication to be running (for QPixmap support)
                // Re-rasterize SVG image to max size
                image = std::make_shared<QImage>(QIcon(path).pixmap(svg_size).toImage());
            } else {
                // Scale image without re-rasterizing it (due to lack of QApplication)
                image = std::make_shared<QImage>(image->scaled(
                        svg_size.width(), svg_size.height(), Qt::KeepAspectRatio, Qt::SmoothTransformation));
            }
        }
    }

    return loaded;
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
