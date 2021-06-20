/**
 * @file
 * @brief Source file for ParametricEQ audio effect class
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

#include "ParametricEQ.h"
#include "Exceptions.h"

using namespace openshot;

/// Blank constructor, useful when using Json to load the effect properties
ParametricEQ::ParametricEQ() : filter_type(LOW_PASS), frequency(500), gain(0), q_factor(0) {
	// Init effect properties
	init_effect_details();
}


// Default constructor
ParametricEQ::ParametricEQ(openshot::FilterType new_filter_type, Keyframe new_frequency, Keyframe new_gain, Keyframe new_q_factor) : 
					   	   filter_type(new_filter_type), frequency(new_frequency), gain(new_gain), q_factor(new_q_factor)
{
	// Init effect properties
	init_effect_details();
}

// Init effect settings
void ParametricEQ::init_effect_details()
{
	/// Initialize the values of the EffectInfo struct.
	InitEffectInfo();

	/// Set the effect info
	info.class_name = "ParametricEQ";
	info.name = "Parametric EQ";
	info.description = "Add equalization on the frame's sound.";
	info.has_audio = true;
	info.has_video = false;
}

// This method is required for all derived classes of EffectBase, and returns a
// modified openshot::Frame object
std::shared_ptr<openshot::Frame> ParametricEQ::GetFrame(std::shared_ptr<openshot::Frame> frame, int64_t frame_number)
{
	filters.clear();

    for (int i = 0; i < frame->audio->getNumChannels(); ++i) {
        Filter* filter;
        filters.add(filter = new Filter());
    }

	const int num_input_channels = frame->audio->getNumChannels();
    const int num_output_channels = frame->audio->getNumChannels();
    const int num_samples = frame->audio->getNumSamples();
    updateFilters(frame_number, num_samples);

	// Add distortion
	for (int channel = 0; channel < frame->audio->getNumChannels(); channel++)
	{
		auto *channel_data = frame->audio->getWritePointer(channel);
		filters[channel]->processSamples(channel_data, num_samples);
	}

    for (int channel = num_input_channels; channel < num_output_channels; ++channel)
	{
        frame->audio->clear(channel, 0, num_samples);
	}

	// return the modified frame
	return frame;
}

void ParametricEQ::updateFilters(int64_t frame_number, double sample_rate)
{
    double discrete_frequency = 2.0 * M_PI * (double)frequency.GetValue(frame_number) / sample_rate;
	double q_value = (double)q_factor.GetValue(frame_number);
	double gain_value = pow(10.0, (double)gain.GetValue(frame_number) * 0.05);
	int filter_type_value = (int)filter_type;

    for (int i = 0; i < filters.size(); ++i)
		filters[i]->updateCoefficients(discrete_frequency, q_value, gain_value, filter_type_value);
}

// Generate JSON string of this object
std::string ParametricEQ::Json() const {

	// Return formatted string
	return JsonValue().toStyledString();
}

// Generate Json::Value for this object
Json::Value ParametricEQ::JsonValue() const {

	// Create root json object
	Json::Value root = EffectBase::JsonValue(); // get parent properties
	root["type"] = info.class_name;
	root["filter_type"] = filter_type;
	root["frequency"] = frequency.JsonValue();;
	root["q_factor"] = q_factor.JsonValue();
	root["gain"] = gain.JsonValue();

	// return JsonValue
	return root;
}

// Load JSON string into this object
void ParametricEQ::SetJson(const std::string value) {

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
void ParametricEQ::SetJsonValue(const Json::Value root) {

	// Set parent data
	EffectBase::SetJsonValue(root);

	// Set data from Json (if key is found)
	if (!root["filter_type"].isNull())
		filter_type = (FilterType)root["filter_type"].asInt();

	if (!root["frequency"].isNull())
		frequency.SetJsonValue(root["frequency"]);

	if (!root["gain"].isNull())
		gain.SetJsonValue(root["gain"]);

	if (!root["q_factor"].isNull())
		q_factor.SetJsonValue(root["q_factor"]);
}

// Get all properties for a specific frame
std::string ParametricEQ::PropertiesJSON(int64_t requested_frame) const {

	// Generate JSON properties list	
	Json::Value root;
	root["id"] = add_property_json("ID", 0.0, "string", Id(), NULL, -1, -1, true, requested_frame);
	root["layer"] = add_property_json("Track", Layer(), "int", "", NULL, 0, 20, false, requested_frame);
	root["start"] = add_property_json("Start", Start(), "float", "", NULL, 0, 1000 * 60 * 30, false, requested_frame);
	root["end"] = add_property_json("End", End(), "float", "", NULL, 0, 1000 * 60 * 30, false, requested_frame);
	root["duration"] = add_property_json("Duration", Duration(), "float", "", NULL, 0, 1000 * 60 * 30, true, requested_frame);

	// Keyframes
	root["filter_type"] = add_property_json("Filter Type", filter_type, "int", "", NULL, 0, 3, false, requested_frame);
	root["frequency"] = add_property_json("Frequency (Hz)", frequency.GetValue(requested_frame), "int", "", &frequency, 20, 20000, false, requested_frame);
	root["gain"] = add_property_json("Gain (dB)", gain.GetValue(requested_frame), "int", "", &gain, -24, 24, false, requested_frame);
	root["q_factor"] = add_property_json("Q Factor", q_factor.GetValue(requested_frame), "float", "", &q_factor, 0, 20, false, requested_frame);

	// Add filter_type choices (dropdown style)
	root["filter_type"]["choices"].append(add_property_choice_json("Low Pass", LOW_PASS, filter_type));
	root["filter_type"]["choices"].append(add_property_choice_json("High Pass", HIGH_PASS, filter_type));
	root["filter_type"]["choices"].append(add_property_choice_json("Low Shelf", LOW_SHELF, filter_type));
	root["filter_type"]["choices"].append(add_property_choice_json("High Shelf", HIGH_SHELF, filter_type));
	root["filter_type"]["choices"].append(add_property_choice_json("Band Pass", BAND_PASS, filter_type));
	root["filter_type"]["choices"].append(add_property_choice_json("Band Stop", BAND_STOP, filter_type));
	root["filter_type"]["choices"].append(add_property_choice_json("Peaking Notch", PEAKING_NOTCH, filter_type));

	// Return formatted string
	return root.toStyledString();
}
