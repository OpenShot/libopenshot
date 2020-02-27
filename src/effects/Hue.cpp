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

	int pixel_count = frame_image->width() * frame_image->height();

	// Get the current hue percentage shift amount, and convert to degrees
	double degrees = 360.0 * hue.GetValue(frame_number);
	float cosA = cos(degrees*3.14159265f/180);
	float sinA = sin(degrees*3.14159265f/180);

	// Calculate a rotation matrix for the RGB colorspace (based on the current hue shift keyframe value)
	float matrix[3] = {
		cosA + (1.0f - cosA) / 3.0f,
		1.0f/3.0f * (1.0f - cosA) - sqrtf(1.0f/3.0f) * sinA,
		1.0f/3.0f * (1.0f - cosA) + sqrtf(1.0f/3.0f) * sinA
	};

	// Loop through pixels
	unsigned char *pixels = (unsigned char *) frame_image->bits();

	#pragma omp parallel for shared (pixels)
	for (int pixel = 0; pixel < pixel_count; ++pixel)
	{
		// Get the RGB values from the pixel (ignore the alpha channel)
		int R = pixels[pixel * 4];
		int G = pixels[pixel * 4 + 1];
		int B = pixels[pixel * 4 + 2];

		// Multiply each color by the hue rotation matrix
		pixels[pixel * 4] = constrain(R * matrix[0] + G * matrix[1] + B * matrix[2]);
		pixels[pixel * 4 + 1] = constrain(R * matrix[2] + G * matrix[0] + B * matrix[1]);
		pixels[pixel * 4 + 2] = constrain(R * matrix[1] + G * matrix[2] + B * matrix[0]);
	}

	// return the modified frame
	return frame;
}

// Generate JSON string of this object
std::string Hue::Json() const {

	// Return formatted string
	return JsonValue().toStyledString();
}

// Generate Json::Value for this object
Json::Value Hue::JsonValue() const {

	// Create root json object
	Json::Value root = EffectBase::JsonValue(); // get parent properties
	root["type"] = info.class_name;
	root["hue"] = hue.JsonValue();

	// return JsonValue
	return root;
}

// Load JSON string into this object
void Hue::SetJson(const std::string value) {

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
void Hue::SetJsonValue(const Json::Value root) {

	// Set parent data
	EffectBase::SetJsonValue(root);

	// Set data from Json (if key is found)
	if (!root["hue"].isNull())
		hue.SetJsonValue(root["hue"]);
}

// Get all properties for a specific frame
std::string Hue::PropertiesJSON(int64_t requested_frame) const {

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
