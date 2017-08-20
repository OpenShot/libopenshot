/**
 * @file
 * @brief Source file for De-interlace class
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

#include "../../include/effects/Deinterlace.h"

using namespace openshot;

/// Blank constructor, useful when using Json to load the effect properties
Deinterlace::Deinterlace() : isOdd(true)
{
	// Init effect properties
	init_effect_details();
}

// Default constructor
Deinterlace::Deinterlace(bool UseOddLines) : isOdd(UseOddLines)
{
	// Init effect properties
	init_effect_details();
}

// Init effect settings
void Deinterlace::init_effect_details()
{
	/// Initialize the values of the EffectInfo struct.
	InitEffectInfo();

	/// Set the effect info
	info.class_name = "Deinterlace";
	info.name = "Deinterlace";
	info.description = "Remove interlacing from a video (i.e. even or odd horizontal lines)";
	info.has_audio = false;
	info.has_video = true;
}

// This method is required for all derived classes of EffectBase, and returns a
// modified openshot::Frame object
std::shared_ptr<Frame> Deinterlace::GetFrame(std::shared_ptr<Frame> frame, long int frame_number)
{
	// Get original size of frame's image
	int original_width = frame->GetImage()->width();
	int original_height = frame->GetImage()->height();

	// Get the frame's image
	std::shared_ptr<QImage> image = frame->GetImage();
	const unsigned char* pixels = image->bits();

	// Create a smaller, new image
	QImage deinterlaced_image(image->width(), image->height() / 2, QImage::Format_RGBA8888);
	const unsigned char* deinterlaced_pixels = deinterlaced_image.bits();

	// Loop through the scanlines of the image (even or odd)
	int start = 0;
	if (isOdd)
		start = 1;
	for (int row = start; row < image->height(); row += 2) {
		memcpy((unsigned char*)deinterlaced_pixels, pixels + (row * image->bytesPerLine()), image->bytesPerLine());
		deinterlaced_pixels += image->bytesPerLine();
	}

	// Resize deinterlaced image back to original size, and update frame's image
	image = std::shared_ptr<QImage>(new QImage(deinterlaced_image.scaled(original_width, original_height, Qt::IgnoreAspectRatio, Qt::FastTransformation)));

	// Update image on frame
	frame->AddImage(image);

	// return the modified frame
	return frame;
}

// Generate JSON string of this object
string Deinterlace::Json() {

	// Return formatted string
	return JsonValue().toStyledString();
}

// Generate Json::JsonValue for this object
Json::Value Deinterlace::JsonValue() {

	// Create root json object
	Json::Value root = EffectBase::JsonValue(); // get parent properties
	root["type"] = info.class_name;
	root["isOdd"] = isOdd;

	// return JsonValue
	return root;
}

// Load JSON string into this object
void Deinterlace::SetJson(string value) throw(InvalidJSON) {

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
void Deinterlace::SetJsonValue(Json::Value root) {

	// Set parent data
	EffectBase::SetJsonValue(root);

	// Set data from Json (if key is found)
	if (!root["isOdd"].isNull())
		isOdd = root["isOdd"].asBool();
}

// Get all properties for a specific frame
string Deinterlace::PropertiesJSON(long int requested_frame) {

	// Generate JSON properties list
	Json::Value root;
	root["id"] = add_property_json("ID", 0.0, "string", Id(), NULL, -1, -1, true, requested_frame);
	root["position"] = add_property_json("Position", Position(), "float", "", NULL, 0, 30 * 60 * 60 * 48, false, requested_frame);
	root["layer"] = add_property_json("Track", Layer(), "int", "", NULL, 0, 20, false, requested_frame);
	root["start"] = add_property_json("Start", Start(), "float", "", NULL, 0, 30 * 60 * 60 * 48, false, requested_frame);
	root["end"] = add_property_json("End", End(), "float", "", NULL, 0, 30 * 60 * 60 * 48, false, requested_frame);
	root["duration"] = add_property_json("Duration", Duration(), "float", "", NULL, 0, 30 * 60 * 60 * 48, true, requested_frame);
	root["isOdd"] = add_property_json("Is Odd Frame", isOdd, "bool", "", NULL, 0, 1, true, requested_frame);

	// Add Is Odd Frame choices (dropdown style)
	root["isOdd"]["choices"].append(add_property_choice_json("Yes", true, isOdd));
	root["isOdd"]["choices"].append(add_property_choice_json("No", false, isOdd));

	// Return formatted string
	return root.toStyledString();
}
