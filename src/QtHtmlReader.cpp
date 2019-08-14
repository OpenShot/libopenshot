/**
 * @file
 * @brief Source file for QtHtmlReader class
 * @author Jonathan Thomas <jonathan@openshot.org>
 * @author Sergei Kolesov (jediserg)
 * @author Jeff Shillitto (jeffski)
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

#include "../include/QtHtmlReader.h"
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
		image = std::shared_ptr<QImage>(new QImage(width, height, QImage::Format_RGBA8888));
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
		info.file_size = 0;
		info.vcodec = "QImage";
		info.width = width;
		info.height = height;
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
	if (image)
	{
		// Create or get frame object
		std::shared_ptr<Frame> image_frame(new Frame(requested_frame, image->size().width(), image->size().height(), background_color, 0, 2));

		// Add Image data to frame
		image_frame->AddImage(image);

		// return frame object
		return image_frame;
	} else {
		// return empty frame
		std::shared_ptr<Frame> image_frame(new Frame(1, 640, 480, background_color, 0, 2));

		// return frame object
		return image_frame;
	}

}

// Generate JSON string of this object
std::string QtHtmlReader::Json() {

	// Return formatted string
	return JsonValue().toStyledString();
}

// Generate Json::JsonValue for this object
Json::Value QtHtmlReader::JsonValue() {

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
void QtHtmlReader::SetJson(std::string value) {

	// Parse JSON string into JSON objects
	Json::Value root;
	Json::CharReaderBuilder rbuilder;
	Json::CharReader* reader(rbuilder.newCharReader());

	std::string errors;
	bool success = reader->parse( value.c_str(),
                 value.c_str() + value.size(), &root, &errors );
	delete reader;
	
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
void QtHtmlReader::SetJsonValue(Json::Value root) {

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
