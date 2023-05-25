/**
 * @file
 * @brief Source file for Hue effect class
 * @author Jonathan Thomas <jonathan@openshot.org>
 *
 * @ref License
 */

// Copyright (c) 2008-2019 OpenShot Studios, LLC
//
// SPDX-License-Identifier: LGPL-3.0-or-later

#include "Hue.h"
#include "Exceptions.h"

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
std::shared_ptr<openshot::Frame> Hue::GetFrame(std::shared_ptr<openshot::Frame> frame, int64_t frame_number)
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
		// Calculate alpha % (to be used for removing pre-multiplied alpha value)
		int A = pixels[pixel * 4 + 3];
		float alpha_percent = A / 255.0;

		// Get RGB values, and remove pre-multiplied alpha
		int R = pixels[pixel * 4 + 0] / alpha_percent;
		int G = pixels[pixel * 4 + 1] / alpha_percent;
		int B = pixels[pixel * 4 + 2] / alpha_percent;

		// Multiply each color by the hue rotation matrix
		pixels[pixel * 4] = constrain(R * matrix[0] + G * matrix[1] + B * matrix[2]);
		pixels[pixel * 4 + 1] = constrain(R * matrix[2] + G * matrix[0] + B * matrix[1]);
		pixels[pixel * 4 + 2] = constrain(R * matrix[1] + G * matrix[2] + B * matrix[0]);

		// Pre-multiply the alpha back into the color channels
		pixels[pixel * 4 + 0] *= alpha_percent;
		pixels[pixel * 4 + 1] *= alpha_percent;
		pixels[pixel * 4 + 2] *= alpha_percent;
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
	Json::Value root = BasePropertiesJSON(requested_frame);

	// Keyframes
	root["hue"] = add_property_json("Hue", hue.GetValue(requested_frame), "float", "", &hue, 0.0, 1.0, false, requested_frame);

	// Return formatted string
	return root.toStyledString();
}
