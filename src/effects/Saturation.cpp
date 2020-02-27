/**
 * @file
 * @brief Source file for Saturation class
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

#include "../../include/effects/Saturation.h"

using namespace openshot;

/// Blank constructor, useful when using Json to load the effect properties
Saturation::Saturation() : saturation(1.0) {
	// Init effect properties
	init_effect_details();
}

// Default constructor
Saturation::Saturation(Keyframe new_saturation) : saturation(new_saturation)
{
	// Init effect properties
	init_effect_details();
}

// Init effect settings
void Saturation::init_effect_details()
{
	/// Initialize the values of the EffectInfo struct.
	InitEffectInfo();

	/// Set the effect info
	info.class_name = "Saturation";
	info.name = "Color Saturation";
	info.description = "Adjust the color saturation.";
	info.has_audio = false;
	info.has_video = true;
}

// This method is required for all derived classes of EffectBase, and returns a
// modified openshot::Frame object
std::shared_ptr<Frame> Saturation::GetFrame(std::shared_ptr<Frame> frame, int64_t frame_number)
{
	// Get the frame's image
	std::shared_ptr<QImage> frame_image = frame->GetImage();

	if (!frame_image)
		return frame;

	int pixel_count = frame_image->width() * frame_image->height();

	// Get keyframe values for this frame
	float saturation_value = saturation.GetValue(frame_number);

	// Constants used for color saturation formula
	const double pR = .299;
	const double pG = .587;
	const double pB = .114;

	// Loop through pixels
	unsigned char *pixels = (unsigned char *) frame_image->bits();

	#pragma omp parallel for shared (pixels)
	for (int pixel = 0; pixel < pixel_count; ++pixel)
	{
		// Get the RGB values from the pixel
		int R = pixels[pixel * 4];
		int G = pixels[pixel * 4 + 1];
		int B = pixels[pixel * 4 + 2];

		// Calculate the saturation multiplier
		double p = sqrt( (R * R * pR) +
		                 (G * G * pG) +
		                 (B * B * pB) );

		// Apply adjusted and constrained saturation
		pixels[pixel * 4]     = constrain(p + (R - p) * saturation_value);
		pixels[pixel * 4 + 1] = constrain(p + (G - p) * saturation_value);
		pixels[pixel * 4 + 2] = constrain(p + (B - p) * saturation_value);
	}

	// return the modified frame
	return frame;
}

// Generate JSON string of this object
std::string Saturation::Json() const {

	// Return formatted string
	return JsonValue().toStyledString();
}

// Generate Json::Value for this object
Json::Value Saturation::JsonValue() const {

	// Create root json object
	Json::Value root = EffectBase::JsonValue(); // get parent properties
	root["type"] = info.class_name;
	root["saturation"] = saturation.JsonValue();

	// return JsonValue
	return root;
}

// Load JSON string into this object
void Saturation::SetJson(const std::string value) {

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
void Saturation::SetJsonValue(const Json::Value root) {

	// Set parent data
	EffectBase::SetJsonValue(root);

	// Set data from Json (if key is found)
	if (!root["saturation"].isNull())
		saturation.SetJsonValue(root["saturation"]);
}

// Get all properties for a specific frame
std::string Saturation::PropertiesJSON(int64_t requested_frame) const {

	// Generate JSON properties list
	Json::Value root;
	root["id"] = add_property_json("ID", 0.0, "string", Id(), NULL, -1, -1, true, requested_frame);
	root["position"] = add_property_json("Position", Position(), "float", "", NULL, 0, 30 * 60 * 60 * 48, false, requested_frame);
	root["layer"] = add_property_json("Track", Layer(), "int", "", NULL, 0, 20, false, requested_frame);
	root["start"] = add_property_json("Start", Start(), "float", "", NULL, 0, 30 * 60 * 60 * 48, false, requested_frame);
	root["end"] = add_property_json("End", End(), "float", "", NULL, 0, 30 * 60 * 60 * 48, false, requested_frame);
	root["duration"] = add_property_json("Duration", Duration(), "float", "", NULL, 0, 30 * 60 * 60 * 48, true, requested_frame);

	// Keyframes
	root["saturation"] = add_property_json("Saturation", saturation.GetValue(requested_frame), "float", "", &saturation, 0.0, 4.0, false, requested_frame);

	// Return formatted string
	return root.toStyledString();
}
