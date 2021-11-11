/**
 * @file
 * @brief Source file for Compressor audio effect class
 * @author
 *
 * @ref License
 */

// Copyright (c) 2008-2019 OpenShot Studios, LLC
//
// SPDX-License-Identifier: LGPL-3.0-or-later

#include "Compressor.h"
#include "Exceptions.h"
#include "Frame.h"

using namespace openshot;

Compressor::Compressor() : Compressor::Compressor(-10, 1, 1, 1, 1, false) {}

Compressor::Compressor(Keyframe threshold, Keyframe ratio, Keyframe attack,
                       Keyframe release, Keyframe makeup_gain,
                       Keyframe bypass):
    threshold(threshold), ratio(ratio), attack(attack),
    release(release), makeup_gain(makeup_gain), bypass(bypass),
    input_level(0.0), yl_prev(0.0)
{
	// Init effect properties
	init_effect_details();
}

// Init effect settings
void Compressor::init_effect_details()
{
	/// Initialize the values of the EffectInfo struct.
	InitEffectInfo();

	/// Set the effect info
	info.class_name = "Compressor";
	info.name = "Compressor";
	info.description = "Reduce the volume of loud sounds or amplify quiet sounds.";
	info.has_audio = true;
	info.has_video = false;
}

// This method is required for all derived classes of EffectBase, and returns a
// modified openshot::Frame object
std::shared_ptr<openshot::Frame> Compressor::GetFrame(std::shared_ptr<openshot::Frame> frame, int64_t frame_number)
{
	// Adding Compressor
    const int num_input_channels = frame->audio->getNumChannels();
    const int num_output_channels = frame->audio->getNumChannels();
    const int num_samples = frame->audio->getNumSamples();

    mixed_down_input.setSize(1, num_samples);
	inverse_sample_rate = 1.0f / frame->SampleRate();
    inverseE = 1.0f / M_E;

	if ((bool)bypass.GetValue(frame_number))
        return frame;

	mixed_down_input.clear();

	for (int channel = 0; channel < num_input_channels; ++channel)
        mixed_down_input.addFrom(0, 0, *frame->audio, channel, 0, num_samples, 1.0f / num_input_channels);

    for (int sample = 0; sample < num_samples; ++sample) {
        float T = threshold.GetValue(frame_number);
        float R = ratio.GetValue(frame_number);
        float alphaA = calculateAttackOrRelease(attack.GetValue(frame_number));
        float alphaR = calculateAttackOrRelease(release.GetValue(frame_number));
        float gain = makeup_gain.GetValue(frame_number);
		float input_squared = powf(mixed_down_input.getSample(0, sample), 2.0f);

		input_level = input_squared;

        xg = (input_level <= 1e-6f) ? -60.0f : 10.0f * log10f(input_level);

		if (xg < T)
			yg = xg;
		else
			yg = T + (xg - T) / R;

		xl = xg - yg;

		if (xl > yl_prev)
			yl = alphaA * yl_prev + (1.0f - alphaA) * xl;
		else
			yl = alphaR * yl_prev + (1.0f - alphaR) * xl;

        control = powf (10.0f, (gain - yl) * 0.05f);
        yl_prev = yl;

        for (int channel = 0; channel < num_input_channels; ++channel) {
            float new_value = frame->audio->getSample(channel, sample)*control;
            frame->audio->setSample(channel, sample, new_value);
        }
	}

    for (int channel = num_input_channels; channel < num_output_channels; ++channel)
        frame->audio->clear(channel, 0, num_samples);

	// return the modified frame
	return frame;
}

float Compressor::calculateAttackOrRelease(float value)
{
    if (value == 0.0f)
        return 0.0f;
    else
        return pow (inverseE, inverse_sample_rate / value);
}

// Generate JSON string of this object
std::string Compressor::Json() const {

	// Return formatted string
	return JsonValue().toStyledString();
}

// Generate Json::Value for this object
Json::Value Compressor::JsonValue() const {

	// Create root json object
	Json::Value root = EffectBase::JsonValue(); // get parent properties
	root["type"] = info.class_name;
	root["threshold"] = threshold.JsonValue();
	root["ratio"] = ratio.JsonValue();
	root["attack"] = attack.JsonValue();
	root["release"] = release.JsonValue();
	root["makeup_gain"] = makeup_gain.JsonValue();
	root["bypass"] = bypass.JsonValue();

	// return JsonValue
	return root;
}

// Load JSON string into this object
void Compressor::SetJson(const std::string value) {

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
void Compressor::SetJsonValue(const Json::Value root) {

	// Set parent data
	EffectBase::SetJsonValue(root);

	// Set data from Json (if key is found)
	if (!root["threshold"].isNull())
		threshold.SetJsonValue(root["threshold"]);

	if (!root["ratio"].isNull())
		ratio.SetJsonValue(root["ratio"]);

	if (!root["attack"].isNull())
		attack.SetJsonValue(root["attack"]);

	if (!root["release"].isNull())
		release.SetJsonValue(root["release"]);

	if (!root["makeup_gain"].isNull())
		makeup_gain.SetJsonValue(root["makeup_gain"]);

	if (!root["bypass"].isNull())
		bypass.SetJsonValue(root["bypass"]);
}

// Get all properties for a specific frame
std::string Compressor::PropertiesJSON(int64_t requested_frame) const {

	// Generate JSON properties list
	Json::Value root;
	root["id"] = add_property_json("ID", 0.0, "string", Id(), NULL, -1, -1, true, requested_frame);
	root["layer"] = add_property_json("Track", Layer(), "int", "", NULL, 0, 20, false, requested_frame);
	root["start"] = add_property_json("Start", Start(), "float", "", NULL, 0, 1000 * 60 * 30, false, requested_frame);
	root["end"] = add_property_json("End", End(), "float", "", NULL, 0, 1000 * 60 * 30, false, requested_frame);
	root["duration"] = add_property_json("Duration", Duration(), "float", "", NULL, 0, 1000 * 60 * 30, true, requested_frame);

	// Keyframes
	root["threshold"] = add_property_json("Threshold (dB)", threshold.GetValue(requested_frame), "float", "", &threshold, -60, 0, false, requested_frame);
	root["ratio"] = add_property_json("Ratio", ratio.GetValue(requested_frame), "float", "", &ratio, 1, 100, false, requested_frame);
	root["attack"] = add_property_json("Attack (ms)", attack.GetValue(requested_frame), "float", "", &attack, 0.1, 100, false, requested_frame);
	root["release"] = add_property_json("Release (ms)", release.GetValue(requested_frame), "float", "", &release, 10, 1000, false, requested_frame);
	root["makeup_gain"] = add_property_json("Makeup gain (dB)", makeup_gain.GetValue(requested_frame), "float", "", &makeup_gain, -12, 12, false, requested_frame);
	root["bypass"] = add_property_json("Bypass", bypass.GetValue(requested_frame), "bool", "", &bypass, 0, 1, false, requested_frame);

	// Return formatted string
	return root.toStyledString();
}
