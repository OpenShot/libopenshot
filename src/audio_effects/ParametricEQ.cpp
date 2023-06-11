/**
 * @file
 * @brief Source file for ParametricEQ audio effect class
 * @author
 *
 * @ref License
 */

// Copyright (c) 2008-2019 OpenShot Studios, LLC
//
// SPDX-License-Identifier: LGPL-3.0-or-later

#include "ParametricEQ.h"
#include "Exceptions.h"

using namespace openshot;
using namespace juce;

ParametricEQ::ParametricEQ(): ParametricEQ::ParametricEQ(LOW_PASS, 500, 0, 0) {}

ParametricEQ::ParametricEQ(openshot::FilterType filter_type,
						   Keyframe frequency, Keyframe gain, Keyframe q_factor) :
	filter_type(filter_type),
	frequency(frequency), gain(gain), q_factor(q_factor)
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
	info.description = "Filter that allows you to adjust the volume level of a frequency in the audio track.";
	info.has_audio = true;
	info.has_video = false;
	initialized = false;
}

// This method is required for all derived classes of EffectBase, and returns a
// modified openshot::Frame object
std::shared_ptr<openshot::Frame> ParametricEQ::GetFrame(std::shared_ptr<openshot::Frame> frame, int64_t frame_number)
{
	if (!initialized)
	{
		filters.clear();

		for (int i = 0; i < frame->audio->getNumChannels(); ++i) {
			Filter *filter;
			filters.add(filter = new Filter());
		}

		initialized = true;
	}

	const int num_input_channels = frame->audio->getNumChannels();
	const int num_output_channels = frame->audio->getNumChannels();
	const int num_samples = frame->audio->getNumSamples();
	updateFilters(frame_number, num_samples);

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

void ParametricEQ::Filter::updateCoefficients (
	const double discrete_frequency,
	const double q_factor,
	const double gain,
	const int filter_type)
{
	double bandwidth = jmin (discrete_frequency / q_factor, M_PI * 0.99);
	double two_cos_wc = -2.0 * cos (discrete_frequency);
	double tan_half_bw = tan (bandwidth / 2.0);
	double tan_half_wc = tan (discrete_frequency / 2.0);
	double sqrt_gain = sqrt (gain);

	switch (filter_type) {
		case 0 /* LOW_PASS */: {
			coefficients = IIRCoefficients (/* b0 */ tan_half_wc,
											/* b1 */ tan_half_wc,
											/* b2 */ 0.0,
											/* a0 */ tan_half_wc + 1.0,
											/* a1 */ tan_half_wc - 1.0,
											/* a2 */ 0.0);
			break;
		}
		case 1 /* HIGH_PASS */: {
			coefficients = IIRCoefficients (/* b0 */ 1.0,
											/* b1 */ -1.0,
											/* b2 */ 0.0,
											/* a0 */ tan_half_wc + 1.0,
											/* a1 */ tan_half_wc - 1.0,
											/* a2 */ 0.0);
			break;
		}
		case 2 /* LOW_SHELF */: {
			coefficients = IIRCoefficients (/* b0 */ gain * tan_half_wc + sqrt_gain,
											/* b1 */ gain * tan_half_wc - sqrt_gain,
											/* b2 */ 0.0,
											/* a0 */ tan_half_wc + sqrt_gain,
											/* a1 */ tan_half_wc - sqrt_gain,
											/* a2 */ 0.0);
			break;
		}
		case 3 /* HIGH_SHELF */: {
			coefficients = IIRCoefficients (/* b0 */ sqrt_gain * tan_half_wc + gain,
											/* b1 */ sqrt_gain * tan_half_wc - gain,
											/* b2 */ 0.0,
											/* a0 */ sqrt_gain * tan_half_wc + 1.0,
											/* a1 */ sqrt_gain * tan_half_wc - 1.0,
											/* a2 */ 0.0);
			break;
		}
		case 4 /* BAND_PASS */: {
			coefficients = IIRCoefficients (/* b0 */ tan_half_bw,
											/* b1 */ 0.0,
											/* b2 */ -tan_half_bw,
											/* a0 */ 1.0 + tan_half_bw,
											/* a1 */ two_cos_wc,
											/* a2 */ 1.0 - tan_half_bw);
			break;
		}
		case 5 /* BAND_STOP */: {
			coefficients = IIRCoefficients (/* b0 */ 1.0,
											/* b1 */ two_cos_wc,
											/* b2 */ 1.0,
											/* a0 */ 1.0 + tan_half_bw,
											/* a1 */ two_cos_wc,
											/* a2 */ 1.0 - tan_half_bw);
			break;
		}
		case 6 /* PEAKING_NOTCH */: {
			coefficients = IIRCoefficients (/* b0 */ sqrt_gain + gain * tan_half_bw,
											/* b1 */ sqrt_gain * two_cos_wc,
											/* b2 */ sqrt_gain - gain * tan_half_bw,
											/* a0 */ sqrt_gain + tan_half_bw,
											/* a1 */ sqrt_gain * two_cos_wc,
											/* a2 */ sqrt_gain - tan_half_bw);
			break;
		}
	}

	setCoefficients(coefficients);
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
	Json::Value root = BasePropertiesJSON(requested_frame);

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
