/**
 * @file
 * @brief Source file for Noise audio effect class
 * @author 
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

#include "Noise.h"
#include "Exceptions.h"

using namespace openshot;

/// Blank constructor, useful when using Json to load the effect properties
Noise::Noise() : level(30) {
	// Init effect properties
	init_effect_details();
}

// Default constructor
Noise::Noise(Keyframe new_level) : level(new_level)
{
	// Init effect properties
	init_effect_details();
}

// Init effect settings
void Noise::init_effect_details()
{
	/// Initialize the values of the EffectInfo struct.
	InitEffectInfo();

	/// Set the effect info
	info.class_name = "Noise";
	info.name = "Noise";
	info.description = "Random signal having equal intensity at different frequencies.";
	info.has_audio = true;
	info.has_video = false;
}

// This method is required for all derived classes of EffectBase, and returns a
// modified openshot::Frame object
std::shared_ptr<openshot::Frame> Noise::GetFrame(std::shared_ptr<openshot::Frame> frame, int64_t frame_number)
{
	// Adding Noise
	srand ( time(NULL) );
	int noise = level.GetValue(frame_number);

	for (int channel = 0; channel < frame->audio->getNumChannels(); channel++)
	{
		auto *buffer = frame->audio->getWritePointer(channel);

		for (auto sample = 0; sample < frame->audio->getNumSamples(); ++sample)
		{
			buffer[sample] = buffer[sample]*(1 - (1+(float)noise)/100) + buffer[sample]*0.0001*(rand()%100+1)*noise;
		}
	}
	

	// return the modified frame
	return frame;
}

// Generate JSON string of this object
std::string Noise::Json() const {

	// Return formatted string
	return JsonValue().toStyledString();
}

// Generate Json::Value for this object
Json::Value Noise::JsonValue() const {

	// Create root json object
	Json::Value root = EffectBase::JsonValue(); // get parent properties
	root["type"] = info.class_name;
	root["level"] = level.JsonValue();

	// return JsonValue
	return root;
}

// Load JSON string into this object
void Noise::SetJson(const std::string value) {

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
void Noise::SetJsonValue(const Json::Value root) {

	// Set parent data
	EffectBase::SetJsonValue(root);

	// Set data from Json (if key is found)
	if (!root["level"].isNull())
		level.SetJsonValue(root["level"]);
}

// Get all properties for a specific frame
std::string Noise::PropertiesJSON(int64_t requested_frame) const {

	// Generate JSON properties list
	Json::Value root;
	root["id"] = add_property_json("ID", 0.0, "string", Id(), NULL, -1, -1, true, requested_frame);
	root["layer"] = add_property_json("Track", Layer(), "int", "", NULL, 0, 20, false, requested_frame);
	root["start"] = add_property_json("Start", Start(), "float", "", NULL, 0, 1000 * 60 * 30, false, requested_frame);
	root["end"] = add_property_json("End", End(), "float", "", NULL, 0, 1000 * 60 * 30, false, requested_frame);
	root["duration"] = add_property_json("Duration", Duration(), "float", "", NULL, 0, 1000 * 60 * 30, true, requested_frame);

	// Keyframes
	root["level"] = add_property_json("Level", level.GetValue(requested_frame), "int", "", &level, 0, 100, false, requested_frame);

	// Return formatted string
	return root.toStyledString();
}
