/**
 * @file
 * @brief Source file for QtHtmlReader class
 * @author Jonathan Thomas <jonathan@openshot.org>
 * @author Sergei Kolesov (jediserg)
 * @author Jeff Shillitto (jeffski)
 *
 * @ref License
 */

// Copyright (c) 2008-2019 OpenShot Studios, LLC
//
// SPDX-License-Identifier: LGPL-3.0-or-later

#include "QtHtmlReader.h"
#include "Exceptions.h"
#include "Frame.h"

#include <QImage>
#include <QPainter>
#include <QTextDocument>
#include <QGuiApplication>
#include <QAbstractTextDocumentLayout>

using namespace openshot;

/// Default constructor (blank text)
QtHtmlReader::QtHtmlReader() : width(1024), height(768), x_offset(0), y_offset(0), html(""), css(""), background_color("#000000"), is_open(false), gravity(GRAVITY_CENTER)
{
	// Open and Close the reader, to populate it's attributes (such as height, width, etc...)
	Open();
	Close();
}

QtHtmlReader::QtHtmlReader(int width, int height, int x_offset, int y_offset, GravityType gravity, std::string html, std::string css, std::string background_color)
: width(width), height(height), x_offset(x_offset), y_offset(y_offset), gravity(gravity), html(html), css(css), background_color(background_color), is_open(false)
{
	// Open and Close the reader, to populate it's attributes (such as height, width, etc...)
	Open();
	Close();
}

// Open reader
void QtHtmlReader::Open()
{
	// Open reader if not already open
	if (!is_open)
	{
		// create image
		image = std::make_shared<QImage>(width, height, QImage::Format_RGBA8888_Premultiplied);
		image->fill(QColor(background_color.c_str()));

		//start painting
		QPainter painter;
		if (!painter.begin(image.get())) {
			return;
		}

		//set background
		painter.setBackground(QBrush(background_color.c_str()));

		//draw text
		QTextDocument text_document;

		//disable redo/undo stack as not needed
		text_document.setUndoRedoEnabled(false);

		//create the HTML/CSS document
		text_document.setTextWidth(width);
		text_document.setDefaultStyleSheet(css.c_str());
		text_document.setHtml(html.c_str());

		int td_height = text_document.documentLayout()->documentSize().height();

 		if (gravity == GRAVITY_TOP_LEFT || gravity == GRAVITY_TOP || gravity == GRAVITY_TOP_RIGHT) {
 			painter.translate(x_offset, y_offset);
 		} else if (gravity == GRAVITY_LEFT || gravity == GRAVITY_CENTER || gravity == GRAVITY_RIGHT) {
 			painter.translate(x_offset, (height - td_height) / 2 + y_offset);
 		} else if (gravity == GRAVITY_BOTTOM_LEFT || gravity == GRAVITY_BOTTOM_RIGHT || gravity == GRAVITY_BOTTOM) {
 			painter.translate(x_offset, height - td_height + y_offset);
 		}

 		if (gravity == GRAVITY_TOP_LEFT || gravity == GRAVITY_LEFT || gravity == GRAVITY_BOTTOM_LEFT) {
 			text_document.setDefaultTextOption(QTextOption(Qt::AlignLeft));
 		} else if (gravity == GRAVITY_CENTER || gravity == GRAVITY_TOP || gravity == GRAVITY_BOTTOM) {
 			text_document.setDefaultTextOption(QTextOption(Qt::AlignHCenter));
 		} else if (gravity == GRAVITY_TOP_RIGHT || gravity == GRAVITY_RIGHT|| gravity == GRAVITY_BOTTOM_RIGHT) {
 			text_document.setDefaultTextOption(QTextOption(Qt::AlignRight));
 		}

 		// Draw image
		text_document.drawContents(&painter);

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

// Close reader
void QtHtmlReader::Close()
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
std::shared_ptr<Frame> QtHtmlReader::GetFrame(int64_t requested_frame)
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
		auto image_frame = std::make_shared<Frame>(
			1, 640, 480, background_color, sample_count, info.channels);

		// return frame object
		return image_frame;
	}
}

// Generate JSON string of this object
std::string QtHtmlReader::Json() const {

	// Return formatted string
	return JsonValue().toStyledString();
}

// Generate Json::Value for this object
Json::Value QtHtmlReader::JsonValue() const {

	// Create root json object
	Json::Value root = ReaderBase::JsonValue(); // get parent properties
	root["type"] = "QtHtmlReader";
	root["width"] = width;
	root["height"] = height;
	root["x_offset"] = x_offset;
	root["y_offset"] = y_offset;
	root["html"] = html;
	root["css"] = css;
	root["background_color"] = background_color;
	root["gravity"] = gravity;

	// return JsonValue
	return root;
}

// Load JSON string into this object
void QtHtmlReader::SetJson(const std::string value) {

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
void QtHtmlReader::SetJsonValue(const Json::Value root) {

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
	if (!root["html"].isNull())
		html = root["html"].asString();
	if (!root["css"].isNull())
		css = root["css"].asString();
	if (!root["background_color"].isNull())
		background_color = root["background_color"].asString();
	if (!root["gravity"].isNull())
 		gravity = (GravityType) root["gravity"].asInt();

	// Re-Open path, and re-init everything (if needed)
	if (is_open)
	{
		Close();
		Open();
	}
}
