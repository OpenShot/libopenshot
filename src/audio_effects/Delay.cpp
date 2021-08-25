/**
 * @file
 * @brief Source file for Delay audio effect class
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

#include "Delay.h"
#include "Exceptions.h"

using namespace openshot;

/// Blank constructor, useful when using Json to load the effect properties
Delay::Delay() : delay_time(1) {
	// Init effect properties
	init_effect_details();
}

// Default constructor
Delay::Delay(Keyframe new_delay_time) : delay_time(new_delay_time)
{
	// Init effect properties
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
	Json::Value root;
	root["id"] = add_property_json("ID", 0.0, "string", Id(), NULL, -1, -1, true, requested_frame);
	root["layer"] = add_property_json("Track", Layer(), "int", "", NULL, 0, 20, false, requested_frame);
	root["start"] = add_property_json("Start", Start(), "float", "", NULL, 0, 1000 * 60 * 30, false, requested_frame);
	root["end"] = add_property_json("End", End(), "float", "", NULL, 0, 1000 * 60 * 30, false, requested_frame);
	root["duration"] = add_property_json("Duration", Duration(), "float", "", NULL, 0, 1000 * 60 * 30, true, requested_frame);

	// Keyframes
	root["delay_time"] = add_property_json("Delay Time", delay_time.GetValue(requested_frame), "float", "", &delay_time, 0, 5, false, requested_frame);

	// Return formatted string
	return root.toStyledString();
}
