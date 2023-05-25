/**
 * @file
 * @brief Source file for Echo audio effect class
 * @author
 *
 * @ref License
 */

// Copyright (c) 2008-2019 OpenShot Studios, LLC
//
// SPDX-License-Identifier: LGPL-3.0-or-later

#include "Echo.h"
#include "Exceptions.h"
#include "Frame.h"

using namespace openshot;

Echo::Echo() : Echo::Echo(0.1, 0.5, 0.5) { }

Echo::Echo(Keyframe echo_time, Keyframe feedback, Keyframe mix) :
	echo_time(echo_time), feedback(feedback), mix(mix)
{
	// Init effect properties
	init_effect_details();
}

// Init effect settings
void Echo::init_effect_details()
{
	/// Initialize the values of the EffectInfo struct.
	InitEffectInfo();

	/// Set the effect info
	info.class_name = "Echo";
	info.name = "Echo";
	info.description = "Reflection of sound with a delay after the direct sound.";
	info.has_audio = true;
	info.has_video = false;
	initialized = false;
}

void Echo::setup(std::shared_ptr<openshot::Frame> frame)
{
	if (!initialized)
	{
		const float max_echo_time = 5;
		echo_buffer_samples = (int)(max_echo_time * (float)frame->SampleRate()) + 1;

		if (echo_buffer_samples < 1)
			echo_buffer_samples = 1;

		echo_buffer_channels = frame->audio->getNumChannels();
		echo_buffer.setSize(echo_buffer_channels, echo_buffer_samples);
		echo_buffer.clear();
		echo_write_position = 0;
		initialized = true;
	}
}

// This method is required for all derived classes of EffectBase, and returns a
// modified openshot::Frame object
std::shared_ptr<openshot::Frame> Echo::GetFrame(std::shared_ptr<openshot::Frame> frame, int64_t frame_number)
{
	const float echo_time_value = (float)echo_time.GetValue(frame_number)*(float)frame->SampleRate();
	const float feedback_value = feedback.GetValue(frame_number);
	const float mix_value = mix.GetValue(frame_number);
	int local_write_position;

	setup(frame);

	for (int channel = 0; channel < frame->audio->getNumChannels(); channel++)
	{
		float *channel_data = frame->audio->getWritePointer(channel);
		float *echo_data = echo_buffer.getWritePointer(channel);
		local_write_position = echo_write_position;

		for (auto sample = 0; sample < frame->audio->getNumSamples(); ++sample)
		{
			const float in = (float)(channel_data[sample]);
			float out = 0.0f;

			float read_position = fmodf((float)local_write_position - echo_time_value + (float)echo_buffer_samples, echo_buffer_samples);
			int local_read_position = floorf(read_position);

			if (local_read_position != local_write_position)
			{
				float fraction = read_position - (float)local_read_position;
				float echoed1 = echo_data[(local_read_position + 0)];
				float echoed2 = echo_data[(local_read_position + 1) % echo_buffer_samples];
				out = (float)(echoed1 + fraction * (echoed2 - echoed1));
				channel_data[sample] = in + mix_value*(out - in);
				echo_data[local_write_position] = in + out*feedback_value;
			}

			if (++local_write_position >= echo_buffer_samples)
				local_write_position -= echo_buffer_samples;
		}
	}

	echo_write_position = local_write_position;

	// return the modified frame
	return frame;
}

// Generate JSON string of this object
std::string Echo::Json() const {

	// Return formatted string
	return JsonValue().toStyledString();
}

// Generate Json::Value for this object
Json::Value Echo::JsonValue() const {

	// Create root json object
	Json::Value root = EffectBase::JsonValue(); // get parent properties
	root["type"] = info.class_name;
	root["echo_time"] = echo_time.JsonValue();
	root["feedback"] = feedback.JsonValue();
	root["mix"] = mix.JsonValue();

	// return JsonValue
	return root;
}

// Load JSON string into this object
void Echo::SetJson(const std::string value) {

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
void Echo::SetJsonValue(const Json::Value root) {

	// Set parent data
	EffectBase::SetJsonValue(root);

	// Set data from Json (if key is found)
	if (!root["echo_time"].isNull())
		echo_time.SetJsonValue(root["echo_time"]);
	if (!root["feedback"].isNull())
		feedback.SetJsonValue(root["feedback"]);
	if (!root["mix"].isNull())
		mix.SetJsonValue(root["mix"]);
}

// Get all properties for a specific frame
std::string Echo::PropertiesJSON(int64_t requested_frame) const {

	// Generate JSON properties list
	Json::Value root = BasePropertiesJSON(requested_frame);

	// Keyframes
	root["echo_time"] = add_property_json("Time", echo_time.GetValue(requested_frame), "float", "", &echo_time, 0, 5, false, requested_frame);
	root["feedback"] = add_property_json("Feedback", feedback.GetValue(requested_frame), "float", "", &feedback, 0, 1, false, requested_frame);
	root["mix"] = add_property_json("Mix", mix.GetValue(requested_frame), "float", "", &mix, 0, 1, false, requested_frame);

	// Return formatted string
	return root.toStyledString();
}
