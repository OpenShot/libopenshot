/**
 * @file
 * @brief Source file for TextReader class
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

#include "../include/TextReader.h"

using namespace openshot;

TextReader::TextReader(int width, int height, int x_offset, int y_offset, GravityType gravity, string text, string font, double size, string text_color, string background_color)
: width(width), height(height), x_offset(x_offset), y_offset(y_offset), text(text), font(font), size(size), text_color(text_color), background_color(background_color), is_open(false), gravity(gravity)
{
	// Init FileInfo struct (clear all values)
	InitFileInfo();

	// Open and Close the reader, to populate it's attributes (such as height, width, etc...)
	Open();
	Close();
}

// Open reader
void TextReader::Open()
{
	// Open reader if not already open
	if (!is_open)
	{
		// create image
		image = tr1::shared_ptr<Magick::Image>(new Magick::Image(Magick::Geometry(width,height), Magick::Color(background_color)));

		// Give image a transparent background color
		image->backgroundColor(Magick::Color("none"));

		// Set gravity (map between OpenShot and ImageMagick)
		switch (gravity)
		{
		case GRAVITY_TOP_LEFT:
			lines.push_back(Magick::DrawableGravity(Magick::NorthWestGravity));
			break;
		case GRAVITY_TOP:
			lines.push_back(Magick::DrawableGravity(Magick::NorthGravity));
			break;
		case GRAVITY_TOP_RIGHT:
			lines.push_back(Magick::DrawableGravity(Magick::NorthEastGravity));
			break;
		case GRAVITY_LEFT:
			lines.push_back(Magick::DrawableGravity(Magick::WestGravity));
			break;
		case GRAVITY_CENTER:
			lines.push_back(Magick::DrawableGravity(Magick::CenterGravity));
			break;
		case GRAVITY_RIGHT:
			lines.push_back(Magick::DrawableGravity(Magick::EastGravity));
			break;
		case GRAVITY_BOTTOM_LEFT:
			lines.push_back(Magick::DrawableGravity(Magick::SouthWestGravity));
			break;
		case GRAVITY_BOTTOM:
			lines.push_back(Magick::DrawableGravity(Magick::SouthGravity));
			break;
		case GRAVITY_BOTTOM_RIGHT:
			lines.push_back(Magick::DrawableGravity(Magick::SouthEastGravity));
			break;
		}

		// Set stroke properties
		lines.push_back(Magick::DrawableStrokeColor(Magick::Color("none")));
		lines.push_back(Magick::DrawableStrokeWidth(0.0));
		lines.push_back(Magick::DrawableFillColor(text_color));
		lines.push_back(Magick::DrawableFont(font));
		lines.push_back(Magick::DrawablePointSize(size));
		lines.push_back(Magick::DrawableText(x_offset, y_offset, text));

		// Draw image
		image->draw(lines);

		// Update image properties
		info.has_audio = false;
		info.has_video = true;
		info.file_size = image->fileSize();
		info.vcodec = image->format();
		info.width = image->size().width();
		info.height = image->size().height();
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
void TextReader::Close()
{
	// Close all objects, if reader is 'open'
	if (is_open)
	{
		// Mark as "closed"
		is_open = false;
	}
}

// Get an openshot::Frame object for a specific frame number of this reader.
tr1::shared_ptr<Frame> TextReader::GetFrame(int requested_frame) throw(ReaderClosed)
{
	if (image)
	{
		// Create or get frame object
		tr1::shared_ptr<Frame> image_frame(new Frame(requested_frame, image->size().width(), image->size().height(), "#000000", 0, 2));
		image_frame->SetSampleRate(44100);

		// Add Image data to frame
		tr1::shared_ptr<Magick::Image> copy_image(new Magick::Image(*image.get()));
		copy_image->modifyImage(); // actually copy the image data to this object
		image_frame->AddImage(copy_image);

		// return frame object
		return image_frame;
	}

}

// Generate Json::JsonValue for this object
Json::Value TextReader::JsonValue() {

	// Create root json object
	Json::Value root = ReaderBase::JsonValue(); // get parent properties
	root["width"] = width;
	root["height"] = height;
	root["x_offset"] = x_offset;
	root["y_offset"] = y_offset;
	root["text"] = text;
	root["font"] = font;
	root["size"] = size;
	root["text_color"] = text_color;
	root["background_color"] = background_color;
	root["gravity"] = gravity;

	// return JsonValue
	return root;
}

// Load Json::JsonValue into this object
void TextReader::Json(Json::Value root) throw(InvalidFile) {

	// Set parent data
	ReaderBase::Json(root);

	// Set data from Json (if key is found)
	if (root["width"] != Json::nullValue)
		width = root["width"].asInt();
	if (root["height"] != Json::nullValue)
		height = root["height"].asInt();
	if (root["x_offset"] != Json::nullValue)
		x_offset = root["x_offset"].asInt();
	if (root["y_offset"] != Json::nullValue)
		y_offset = root["y_offset"].asInt();
	if (root["text"] != Json::nullValue)
		text = root["text"].asString();
	if (root["font"] != Json::nullValue)
		font = root["font"].asString();
	if (root["size"] != Json::nullValue)
		size = root["size"].asDouble();
	if (root["text_color"] != Json::nullValue)
		text_color = root["text_color"].asString();
	if (root["background_color"] != Json::nullValue)
		background_color = root["background_color"].asString();
	if (root["gravity"] != Json::nullValue)
		gravity = (GravityType) root["gravity"].asInt();

	// Open path, and re-init everything
	Close();
	Open();
}
