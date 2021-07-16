/**
 * @file
 * @brief Source file for Echo audio effect class
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

#include "Echo.h"
#include "Exceptions.h"

using namespace openshot;

/// Blank constructor, useful when using Json to load the effect properties
Echo::Echo() : echo_time(0.1), feedback(0.5), mix(0.5) {
	// Init effect properties
	init_effect_details();
}

// Default constructor
Echo::Echo(Keyframe new_echo_time, Keyframe new_feedback, Keyframe new_mix) : 
			 echo_time(new_echo_time), feedback(new_feedback), mix(new_mix)
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
	info.description = "Add echo on the frame's sound.";
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
	Json::Value root;
	root["id"] = add_property_json("ID", 0.0, "string", Id(), NULL, -1, -1, true, requested_frame);
	root["layer"] = add_property_json("Track", Layer(), "int", "", NULL, 0, 20, false, requested_frame);
	root["start"] = add_property_json("Start", Start(), "float", "", NULL, 0, 1000 * 60 * 30, false, requested_frame);
	root["end"] = add_property_json("End", End(), "float", "", NULL, 0, 1000 * 60 * 30, false, requested_frame);
	root["duration"] = add_property_json("Duration", Duration(), "float", "", NULL, 0, 1000 * 60 * 30, true, requested_frame);

	// Keyframes
	root["echo_time"] = add_property_json("Time", echo_time.GetValue(requested_frame), "float", "", &echo_time, 0, 5, false, requested_frame);
	root["feedback"] = add_property_json("Feedback", feedback.GetValue(requested_frame), "float", "", &feedback, 0, 1, false, requested_frame);
	root["mix"] = add_property_json("Mix", mix.GetValue(requested_frame), "float", "", &mix, 0, 1, false, requested_frame);

	// Return formatted string
	return root.toStyledString();
}
