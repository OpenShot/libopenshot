/**
 * @file
 * @brief Source file for Hue effect class
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

#include "../../include/effects/Hue.h"

using namespace openshot;

/// Blank constructor, useful when using Json to load the effect properties
Hue::Hue() : Hue(0.0) {
	// Init effect properties
	init_effect_details();
}

// Default constructor
Hue::Hue(Keyframe hue): hue(hue)
{
	// Init effect properties
	init_effect_details();
}

// Init effect settings
void Hue::init_effect_details()
{
	/// Initialize the values of the EffectInfo struct.
	InitEffectInfo();

	/// Set the effect info
	info.class_name = "Hue";
	info.name = "Hue";
	info.description = "Adjust the hue / color of the frame's image.";
	info.has_audio = false;
	info.has_video = true;
}

// This method is required for all derived classes of EffectBase, and returns a
// modified openshot::Frame object
std::shared_ptr<Frame> Hue::GetFrame(std::shared_ptr<Frame> frame, int64_t frame_number)
{
	// Get the frame's image
	std::shared_ptr<QImage> frame_image = frame->GetImage();

	// Get the current hue percentage shift amount, and convert to degrees
	double degrees = 360.0 * hue.GetValue(frame_number);
	float cosA = cos(degrees*3.14159265f/180);
	float sinA = sin(degrees*3.14159265f/180);

	// Calculate a rotation matrix for the RGB colorspace (based on the current hue shift keyframe value)
	float matrix[3][3] = {{cosA + (1.0f - cosA) / 3.0f, 1.0f/3.0f * (1.0f - cosA) - sqrtf(1.0f/3.0f) * sinA, 1.0f/3.0f * (1.0f - cosA) + sqrtf(1.0f/3.0f) * sinA},
						  {1.0f/3.0f * (1.0f - cosA) + sqrtf(1.0f/3.0f) * sinA, cosA + 1.0f/3.0f*(1.0f - cosA), 1.0f/3.0f * (1.0f - cosA) - sqrtf(1.0f/3.0f) * sinA},
						  {1.0f/3.0f * (1.0f - cosA) - sqrtf(1.0f/3.0f) * sinA, 1.0f/3.0f * (1.0f - cosA) + sqrtf(1.0f/3.0f) * sinA, cosA + 1.0f/3.0f * (1.0f - cosA)}};

	// Loop through pixels
	unsigned char *pixels = (unsigned char *) frame_image->bits();
	for (int pixel = 0, byte_index=0; pixel < frame_image->width() * frame_image->height(); pixel++, byte_index+=4)
	{
		// Get the RGB values from the pixel
		int R = pixels[byte_index];
		int G = pixels[byte_index + 1];
		int B = pixels[byte_index + 2];
		int A = pixels[byte_index + 3];

		// Multiply each color by the hue rotation matrix
		float rx = constrain(R * matrix[0][0] + G * matrix[0][1] + B * matrix[0][2]);
		float gx = constrain(R * matrix[1][0] + G * matrix[1][1] + B * matrix[1][2]);
		float bx = constrain(R * matrix[2][0] + G * matrix[2][1] + B * matrix[2][2]);

		// Set all pixels to new value
		pixels[byte_index] = rx;
		pixels[byte_index + 1] = gx;
		pixels[byte_index + 2] = bx;
		pixels[byte_index + 3] = A; // leave the alpha value alone
	}

	// return the modified frame
	return frame;
}

// Generate JSON string of this object
string Hue::Json() {

	// Return formatted string
	return JsonValue().toStyledString();
}

// Generate Json::JsonValue for this object
Json::Value Hue::JsonValue() {

	// Create root json object
	Json::Value root = EffectBase::JsonValue(); // get parent properties
	root["type"] = info.class_name;
	root["hue"] = hue.JsonValue();

	// return JsonValue
	return root;
}

// Load JSON string into this object
void Hue::SetJson(string value) {

	// Parse JSON string into JSON objects
	Json::Value root;
	Json::CharReaderBuilder rbuilder;
	Json::CharReader* reader(rbuilder.newCharReader());

	string errors;
	bool success = reader->parse( value.c_str(),
                 value.c_str() + value.size(), &root, &errors );
	delete reader;

	if (!success)
		// Raise exception
		throw InvalidJSON("JSON could not be parsed (or is invalid)");

	try
	{
		// Set all values that match
		SetJsonValue(root);
	}
	catch (const std::exception& e)
	{
		// Error parsing JSON (or missing keys)
		throw InvalidJSON("JSON is invalid (missing keys or invalid data types)");
	}
}

// Load Json::JsonValue into this object
void Hue::SetJsonValue(Json::Value root) {

	// Set parent data
	EffectBase::SetJsonValue(root);

	// Set data from Json (if key is found)
	if (!root["hue"].isNull())
		hue.SetJsonValue(root["hue"]);
}

// Get all properties for a specific frame
string Hue::PropertiesJSON(int64_t requested_frame) {

	// Generate JSON properties list
	Json::Value root;
	root["id"] = add_property_json("ID", 0.0, "string", Id(), NULL, -1, -1, true, requested_frame);
	root["position"] = add_property_json("Position", Position(), "float", "", NULL, 0, 1000 * 60 * 30, false, requested_frame);
	root["layer"] = add_property_json("Track", Layer(), "int", "", NULL, 0, 20, false, requested_frame);
	root["start"] = add_property_json("Start", Start(), "float", "", NULL, 0, 1000 * 60 * 30, false, requested_frame);
	root["end"] = add_property_json("End", End(), "float", "", NULL, 0, 1000 * 60 * 30, false, requested_frame);
	root["duration"] = add_property_json("Duration", Duration(), "float", "", NULL, 0, 1000 * 60 * 30, true, requested_frame);

	// Keyframes
	root["hue"] = add_property_json("Hue", hue.GetValue(requested_frame), "float", "", &hue, 0.0, 1.0, false, requested_frame);

	// Return formatted string
	return root.toStyledString();
}
