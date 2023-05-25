/**
 * @file
 * @brief Source file for Delay audio effect class
 * @author
 *
 * @ref License
 */

// Copyright (c) 2008-2019 OpenShot Studios, LLC
//
// SPDX-License-Identifier: LGPL-3.0-or-later

#include "Delay.h"
#include "Exceptions.h"
#include "Frame.h"

using namespace openshot;

Delay::Delay() : Delay::Delay(1) { }

Delay::Delay(Keyframe delay_time) : delay_time(delay_time)
{
	init_effect_details();
}

// Init effect settings
void Delay::init_effect_details()
{
	/// Initialize the values of the EffectInfo struct.
	InitEffectInfo();

	/// Set the effect info
	info.class_name = "Delay";
	info.name = "Delay";
	info.description = "Adjust the synchronism between the audio and video track.";
	info.has_audio = true;
	info.has_video = false;
	initialized = false;
}

void Delay::setup(std::shared_ptr<openshot::Frame> frame)
{
	if (!initialized)
	{
		const float max_delay_time = 5;
		delay_buffer_samples = (int)(max_delay_time * (float)frame->SampleRate()) + 1;

		if (delay_buffer_samples < 1)
			delay_buffer_samples = 1;

		delay_buffer_channels = frame->audio->getNumChannels();
		delay_buffer.setSize(delay_buffer_channels, delay_buffer_samples);
		delay_buffer.clear();
		delay_write_position = 0;
		initialized = true;
	}
}

// This method is required for all derived classes of EffectBase, and returns a
// modified openshot::Frame object
std::shared_ptr<openshot::Frame> Delay::GetFrame(std::shared_ptr<openshot::Frame> frame, int64_t frame_number)
{
	const float delay_time_value = (float)delay_time.GetValue(frame_number)*(float)frame->SampleRate();
	int local_write_position;

	setup(frame);

	for (int channel = 0; channel < frame->audio->getNumChannels(); channel++)
	{
		float *channel_data = frame->audio->getWritePointer(channel);
		float *delay_data = delay_buffer.getWritePointer(channel);
		local_write_position = delay_write_position;

		for (auto sample = 0; sample < frame->audio->getNumSamples(); ++sample)
		{
			const float in = (float)(channel_data[sample]);
			float out = 0.0f;

			float read_position = fmodf((float)local_write_position - delay_time_value + (float)delay_buffer_samples, delay_buffer_samples);
			int local_read_position = floorf(read_position);

			if (local_read_position != local_write_position)
			{
				float fraction = read_position - (float)local_read_position;
				float delayed1 = delay_data[(local_read_position + 0)];
				float delayed2 = delay_data[(local_read_position + 1) % delay_buffer_samples];
				out = (float)(delayed1 + fraction * (delayed2 - delayed1));

				channel_data[sample] = in + (out - in);
				delay_data[local_write_position] = in;
			}

			if (++local_write_position >= delay_buffer_samples)
				local_write_position -= delay_buffer_samples;
		}
	}

	delay_write_position = local_write_position;

	// return the modified frame
	return frame;
}

// Generate JSON string of this object
std::string Delay::Json() const {

	// Return formatted string
	return JsonValue().toStyledString();
}

// Generate Json::Value for this object
Json::Value Delay::JsonValue() const {

	// Create root json object
	Json::Value root = EffectBase::JsonValue(); // get parent properties
	root["type"] = info.class_name;
	root["delay_time"] = delay_time.JsonValue();

	// return JsonValue
	return root;
}

// Load JSON string into this object
void Delay::SetJson(const std::string value) {

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
void Delay::SetJsonValue(const Json::Value root) {

	// Set parent data
	EffectBase::SetJsonValue(root);

	// Set data from Json (if key is found)
	if (!root["delay_time"].isNull())
		delay_time.SetJsonValue(root["delay_time"]);
}

// Get all properties for a specific frame
std::string Delay::PropertiesJSON(int64_t requested_frame) const {

	// Generate JSON properties list
	Json::Value root = BasePropertiesJSON(requested_frame);

	// Keyframes
	root["delay_time"] = add_property_json("Delay Time", delay_time.GetValue(requested_frame), "float", "", &delay_time, 0, 5, false, requested_frame);

	// Return formatted string
	return root.toStyledString();
}
