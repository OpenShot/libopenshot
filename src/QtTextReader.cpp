/**
 * @file
 * @brief Source file for QtTextReader class
 * @author Jonathan Thomas <jonathan@openshot.org>
 * @author Sergei Kolesov (jediserg)
 * @author Jeff Shillitto (jeffski)
 *
 * @ref License
 */

// Copyright (c) 2008-2019 OpenShot Studios, LLC
//
// SPDX-License-Identifier: LGPL-3.0-or-later

#include "QtTextReader.h"
#include "CacheBase.h"
#include "Exceptions.h"
#include "Frame.h"

#include <QImage>
#include <QPainter>

using namespace openshot;

/// Default constructor (blank text)
QtTextReader::QtTextReader() : width(1024), height(768), x_offset(0), y_offset(0), text(""), font(QFont("Arial", 10)), text_color("#ffffff"), background_color("#000000"), is_open(false), gravity(GRAVITY_CENTER)
{
	// Open and Close the reader, to populate it's attributes (such as height, width, etc...)
	Open();
	Close();
}

QtTextReader::QtTextReader(int width, int height, int x_offset, int y_offset, GravityType gravity, std::string text, QFont font, std::string text_color, std::string background_color)
: width(width), height(height), x_offset(x_offset), y_offset(y_offset), text(text), font(font), text_color(text_color), background_color(background_color), is_open(false), gravity(gravity)
{
	// Open and Close the reader, to populate it's attributes (such as height, width, etc...)
	Open();
	Close();
}

void QtTextReader::SetTextBackgroundColor(std::string color) {
	text_background_color = color;

	// Open and Close the reader, to populate it's attributes (such as height, width, etc...) plus the text background color
	Open();
	Close();
}

// Open reader
void QtTextReader::Open()
{
	// Open reader if not already open
	if (!is_open)
	{
		// create image
		image = std::make_shared<QImage>(width, height, QImage::Format_RGBA8888_Premultiplied);
		image->fill(QColor(background_color.c_str()));

		QPainter painter;
		if (!painter.begin(image.get())) {
			return;
		}

		// set background
		if (!text_background_color.empty()) {
			painter.setBackgroundMode(Qt::OpaqueMode);
			painter.setBackground(QBrush(text_background_color.c_str()));
		}

		// set font color
		painter.setPen(QPen(text_color.c_str()));

		// set font
		painter.setFont(font);

		// Set gravity (map between OpenShot and Qt)
		int align_flag = 0;
		switch (gravity)
		{
		case GRAVITY_TOP_LEFT:
			align_flag = Qt::AlignLeft | Qt::AlignTop;
			break;
		case GRAVITY_TOP:
			align_flag = Qt::AlignHCenter | Qt::AlignTop;
			break;
		case GRAVITY_TOP_RIGHT:
			align_flag = Qt::AlignRight | Qt::AlignTop;
			break;
		case GRAVITY_LEFT:
			align_flag = Qt::AlignVCenter | Qt::AlignLeft;
			break;
		case GRAVITY_CENTER:
			align_flag = Qt::AlignCenter;
			break;
		case GRAVITY_RIGHT:
			align_flag = Qt::AlignVCenter | Qt::AlignRight;
			break;
		case GRAVITY_BOTTOM_LEFT:
			align_flag = Qt::AlignLeft | Qt::AlignBottom;
			break;
		case GRAVITY_BOTTOM:
			align_flag = Qt::AlignHCenter | Qt::AlignBottom;
			break;
		case GRAVITY_BOTTOM_RIGHT:
			align_flag = Qt::AlignRight | Qt::AlignBottom;
			break;
		}

		// Draw image
		painter.drawText(x_offset, y_offset, width, height, align_flag, text.c_str());

		painter.end();

		// Update image properties
		info.has_audio = false;
		info.has_video = true;
		info.has_single_image = true;
		info.file_size = 0;
		info.vcodec = "QImage";
		info.width = width;
		info.height = height;
		info.pixel_ratio.num = 1;
		info.pixel_ratio.den = 1;
		info.duration = 60 * 60 * 1;  // 1 hour duration
		info.fps.num = 30;
		info.fps.den = 1;
		info.video_timebase.num = 1;
		info.video_timebase.den = 30;
		info.video_length = round(info.duration * info.fps.ToDouble());

		// Calculate the DAR (display aspect ratio)
		Fraction font_size(info.width * info.pixel_ratio.num, info.height * info.pixel_ratio.den);

		// Reduce size fraction
		font_size.Reduce();

		// Set the ratio based on the reduced fraction
		info.display_ratio.num = font_size.num;
		info.display_ratio.den = font_size.den;

		// Mark as "open"
		is_open = true;
	}
}

// Close reader
void QtTextReader::Close()
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
std::shared_ptr<Frame> QtTextReader::GetFrame(int64_t requested_frame)
{
	// Create a scoped lock, allowing only a single thread to run the following code at one time
	const std::lock_guard<std::recursive_mutex> lock(getFrameMutex);

	auto sample_count = Frame::GetSamplesPerFrame(requested_frame, info.fps, info.sample_rate, info.channels);

	if (image)
	{
		// Create or get frame object
		auto image_frame = std::make_shared<Frame>(
			requested_frame, image->size().width(), image->size().height(),
			background_color, sample_count, info.channels);

		// Add Image data to frame
		image_frame->AddImage(image);

		// return frame object
		return image_frame;
	} else {
		// return empty frame
		auto image_frame = std::make_shared<Frame>(1, 640, 480, background_color, sample_count, info.channels);

		// return frame object
		return image_frame;
	}
}

// Generate JSON string of this object
std::string QtTextReader::Json() const {

	// Return formatted string
	return JsonValue().toStyledString();
}

// Generate Json::Value for this object
Json::Value QtTextReader::JsonValue() const {

	// Create root json object
	Json::Value root = ReaderBase::JsonValue(); // get parent properties
	root["type"] = "QtTextReader";
	root["width"] = width;
	root["height"] = height;
	root["x_offset"] = x_offset;
	root["y_offset"] = y_offset;
	root["text"] = text;
	root["font"] = font.toString().toStdString();
	root["text_color"] = text_color;
	root["background_color"] = background_color;
	root["text_background_color"] = text_background_color;
	root["gravity"] = gravity;

	// return JsonValue
	return root;
}

// Load JSON string into this object
void QtTextReader::SetJson(const std::string value) {

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
void QtTextReader::SetJsonValue(const Json::Value root) {

	// Set parent data
	ReaderBase::SetJsonValue(root);

	// Set data from Json (if key is found)
	if (!root["width"].isNull())
		width = root["width"].asInt();
	if (!root["height"].isNull())
		height = root["height"].asInt();
	if (!root["x_offset"].isNull())
		x_offset = root["x_offset"].asInt();
	if (!root["y_offset"].isNull())
		y_offset = root["y_offset"].asInt();
	if (!root["text"].isNull())
		text = root["text"].asString();
	if (!root["font"].isNull())
		font.fromString(QString::fromStdString(root["font"].asString()));
	if (!root["text_color"].isNull())
		text_color = root["text_color"].asString();
	if (!root["background_color"].isNull())
		background_color = root["background_color"].asString();
	if (!root["text_background_color"].isNull())
		text_background_color = root["text_background_color"].asString();
	if (!root["gravity"].isNull())
		gravity = (GravityType) root["gravity"].asInt();

	// Re-Open path, and re-init everything (if needed)
	if (is_open)
	{
		Close();
		Open();
	}
}
