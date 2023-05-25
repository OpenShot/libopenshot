/**
 * @file
 * @brief Source file for Negate class
 * @author Jonathan Thomas <jonathan@openshot.org>
 *
 * @ref License
 */

// Copyright (c) 2008-2019 OpenShot Studios, LLC
//
// SPDX-License-Identifier: LGPL-3.0-or-later

#include "Negate.h"
#include "Exceptions.h"

using namespace openshot;

// Default constructor
Negate::Negate()
{
	/// Initialize the values of the EffectInfo struct.
	InitEffectInfo();

	/// Set the effect info
	info.class_name = "Negate";
	info.name = "Negative";
	info.description = "Negates the colors, producing a negative of the image.";
	info.has_audio = false;
	info.has_video = true;
}

// This method is required for all derived classes of EffectBase, and returns a
// modified openshot::Frame object
std::shared_ptr<openshot::Frame> Negate::GetFrame(std::shared_ptr<openshot::Frame> frame, int64_t frame_number)
{
	// Make a negative of the images pixels
	frame->GetImage()->invertPixels();

	// return the modified frame
	return frame;
}

// Generate JSON string of this object
std::string Negate::Json() const {

	// Return formatted string
	return JsonValue().toStyledString();
}

// Generate Json::Value for this object
Json::Value Negate::JsonValue() const {

	// Create root json object
	Json::Value root = EffectBase::JsonValue(); // get parent properties
	root["type"] = info.class_name;

	// return JsonValue
	return root;
}

// Load JSON string into this object
void Negate::SetJson(const std::string value) {

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
void Negate::SetJsonValue(const Json::Value root) {

	// Set parent data
	EffectBase::SetJsonValue(root);

}

// Get all properties for a specific frame
std::string Negate::PropertiesJSON(int64_t requested_frame) const {

	// Generate JSON properties list
	Json::Value root = BasePropertiesJSON(requested_frame);

	// Return formatted string
	return root.toStyledString();
}
