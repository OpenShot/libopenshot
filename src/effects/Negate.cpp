/**
 * @file
 * @brief Source file for Negate class
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

#include "Negate.h"

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
std::shared_ptr<Frame> Negate::GetFrame(std::shared_ptr<Frame> frame, int64_t frame_number)
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
	Json::Value root;
	root["id"] = add_property_json("ID", 0.0, "string", Id(), NULL, -1, -1, true, requested_frame);
	root["position"] = add_property_json("Position", Position(), "float", "", NULL, 0, 30 * 60 * 60 * 48, false, requested_frame);
	root["layer"] = add_property_json("Track", Layer(), "int", "", NULL, 0, 20, false, requested_frame);
	root["start"] = add_property_json("Start", Start(), "float", "", NULL, 0, 30 * 60 * 60 * 48, false, requested_frame);
	root["end"] = add_property_json("End", End(), "float", "", NULL, 0, 30 * 60 * 60 * 48, false, requested_frame);
	root["duration"] = add_property_json("Duration", Duration(), "float", "", NULL, 0, 30 * 60 * 60 * 48, true, requested_frame);

	// Return formatted string
	return root.toStyledString();
}
