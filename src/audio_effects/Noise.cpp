/**
 * @file
 * @brief Source file for Noise audio effect class
 * @author
 *
 * @ref License
 */

// Copyright (c) 2008-2019 OpenShot Studios, LLC
//
// SPDX-License-Identifier: LGPL-3.0-or-later

#include "Noise.h"
#include "Exceptions.h"
#include "Frame.h"

#include <AppConfig.h>
#include <juce_audio_basics/juce_audio_basics.h>

using namespace openshot;

Noise::Noise(): Noise::Noise(30) { }

Noise::Noise(Keyframe level) : level(level)
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
	Json::Value root = BasePropertiesJSON(requested_frame);

	// Keyframes
	root["level"] = add_property_json("Level", level.GetValue(requested_frame), "int", "", &level, 0, 100, false, requested_frame);

	// Return formatted string
	return root.toStyledString();
}
