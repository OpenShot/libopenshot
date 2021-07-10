/**
 * @file
 * @brief Source file for Distortion audio effect class
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

#include "Distortion.h"
#include "Exceptions.h"

using namespace openshot;

/// Blank constructor, useful when using Json to load the effect properties
Distortion::Distortion() : distortion_type(HARD_CLIPPING), input_gain(10), output_gain(-10), tone(5) {
	// Init effect properties
	init_effect_details();
}

// Default constructor
Distortion::Distortion(openshot::DistortionType new_distortion_type, Keyframe new_input_gain, Keyframe new_output_gain, Keyframe new_tone) : 
					   distortion_type(new_distortion_type), input_gain(new_input_gain), output_gain(new_output_gain), tone(new_tone)
{
	// Init effect properties
	init_effect_details();
}

// Init effect settings
void Distortion::init_effect_details()
{
	/// Initialize the values of the EffectInfo struct.
	InitEffectInfo();

	/// Set the effect info
	info.class_name = "Distortion";
	info.name = "Distortion";
	info.description = "Add distortion on the frame's audio. This effect alters the audio by clipping the signal.";
	info.has_audio = true;
	info.has_video = false;
}


// This method is required for all derived classes of EffectBase, and returns a
// modified openshot::Frame object
std::shared_ptr<openshot::Frame> Distortion::GetFrame(std::shared_ptr<openshot::Frame> frame, int64_t frame_number)
{
	filters.clear();

    for (int i = 0; i < frame->audio->getNumChannels(); ++i) {
        Filter* filter;
        filters.add (filter = new Filter());
    }

    updateFilters(frame_number);

	// Add distortion
	for (int channel = 0; channel < frame->audio->getNumChannels(); channel++)
	{
		auto *channel_data = frame->audio->getWritePointer(channel);
		float out;

		for (auto sample = 0; sample < frame->audio->getNumSamples(); ++sample)
		{

			const int input_gain_value = (int)input_gain.GetValue(frame_number);
			const int output_gain_value = (int)output_gain.GetValue(frame_number);
			const float in = channel_data[sample]*powf(10.0f, input_gain_value * 0.05f);
			
			// Use the current distortion type
			switch (distortion_type) {
				
				case HARD_CLIPPING: {
					float threshold = 0.5f;
                    if (in > threshold)
                        out = threshold;
                    else if (in < -threshold)
                        out = -threshold;
                    else
                        out = in;
                    break;
                }

                case SOFT_CLIPPING: {
                    float threshold1 = 1.0f / 3.0f;
                    float threshold2 = 2.0f / 3.0f;
                    if (in > threshold2)
                        out = 1.0f;
                    else if (in > threshold1)
                        out = 1.0f - powf (2.0f - 3.0f * in, 2.0f) / 3.0f;
                    else if (in < -threshold2)
                        out = -1.0f;
                    else if (in < -threshold1)
                        out = -1.0f + powf (2.0f + 3.0f * in, 2.0f) / 3.0f;
                    else
                        out = 2.0f * in;
                    out *= 0.5f;
                    break;
                }

                case EXPONENTIAL: {
                    if (in > 0.0f)
                        out = 1.0f - expf (-in);
                    else
                        out = -1.0f + expf (in);
                    break;
                }

                case FULL_WAVE_RECTIFIER: {
                    out = fabsf (in);
                    break;
                }

                case HALF_WAVE_RECTIFIER: {
                    if (in > 0.0f)
                        out = in;
                    else
                        out = 0.0f;
                    break;
                }
            }

			float filtered = filters[channel]->processSingleSampleRaw(out);
			channel_data[sample] = filtered*powf(10.0f, output_gain_value * 0.05f);
		}
	}

	// return the modified frame
	return frame;
}

void Distortion::updateFilters(int64_t frame_number)
{
    double discrete_frequency = M_PI * 0.01;
    double gain = pow(10.0, (float)tone.GetValue(frame_number) * 0.05);

    for (int i = 0; i < filters.size(); ++i)
        filters[i]->updateCoefficients(discrete_frequency, gain);
}

// Generate JSON string of this object
std::string Distortion::Json() const {

	// Return formatted string
	return JsonValue().toStyledString();
}

void Distortion::Filter::updateCoefficients(const double discrete_frequency, const double gain)
{
	double tan_half_wc = tan(discrete_frequency / 2.0);
	double sqrt_gain = sqrt(gain);

	coefficients = juce::IIRCoefficients(/* b0 */ sqrt_gain * tan_half_wc + gain,
										 /* b1 */ sqrt_gain * tan_half_wc - gain,
										 /* b2 */ 0.0,
										 /* a0 */ sqrt_gain * tan_half_wc + 1.0,
										 /* a1 */ sqrt_gain * tan_half_wc - 1.0,
										 /* a2 */ 0.0);
	setCoefficients(coefficients);
}

// Generate Json::Value for this object
Json::Value Distortion::JsonValue() const {

	// Create root json object
	Json::Value root = EffectBase::JsonValue(); // get parent properties
	root["type"] = info.class_name;
	root["distortion_type"] = distortion_type;
	root["input_gain"] = input_gain.JsonValue();
	root["output_gain"] = output_gain.JsonValue();
	root["tone"] = tone.JsonValue();

	// return JsonValue
	return root;
}

// Load JSON string into this object
void Distortion::SetJson(const std::string value) {

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
void Distortion::SetJsonValue(const Json::Value root) {

	// Set parent data
	EffectBase::SetJsonValue(root);

	// Set data from Json (if key is found)
	if (!root["distortion_type"].isNull())
		distortion_type = (DistortionType)root["distortion_type"].asInt();

	if (!root["input_gain"].isNull())
		input_gain.SetJsonValue(root["input_gain"]);

	if (!root["output_gain"].isNull())
		output_gain.SetJsonValue(root["output_gain"]);

	if (!root["tone"].isNull())
		tone.SetJsonValue(root["tone"]);
}

// Get all properties for a specific frame
std::string Distortion::PropertiesJSON(int64_t requested_frame) const {

	// Generate JSON properties list
	Json::Value root;
	root["id"] = add_property_json("ID", 0.0, "string", Id(), NULL, -1, -1, true, requested_frame);
	root["layer"] = add_property_json("Track", Layer(), "int", "", NULL, 0, 20, false, requested_frame);
	root["start"] = add_property_json("Start", Start(), "float", "", NULL, 0, 1000 * 60 * 30, false, requested_frame);
	root["end"] = add_property_json("End", End(), "float", "", NULL, 0, 1000 * 60 * 30, false, requested_frame);
	root["duration"] = add_property_json("Duration", Duration(), "float", "", NULL, 0, 1000 * 60 * 30, true, requested_frame);

	// Keyframes
	root["distortion_type"] = add_property_json("Distortion Type", distortion_type, "int", "", NULL, 0, 3, false, requested_frame);
	root["input_gain"] = add_property_json("Input Gain (dB)", input_gain.GetValue(requested_frame), "int", "", &input_gain, -24, 24, false, requested_frame);
	root["output_gain"] = add_property_json("Output Gain (dB)", output_gain.GetValue(requested_frame), "int", "", &output_gain, -24, 24, false, requested_frame);
	root["tone"] = add_property_json("Tone (dB)", tone.GetValue(requested_frame), "int", "", &tone, -24, 24, false, requested_frame);

	// Add distortion_type choices (dropdown style)
	root["distortion_type"]["choices"].append(add_property_choice_json("Hard Clipping", HARD_CLIPPING, distortion_type));
	root["distortion_type"]["choices"].append(add_property_choice_json("Soft Clipping", SOFT_CLIPPING, distortion_type));
	root["distortion_type"]["choices"].append(add_property_choice_json("Exponential", EXPONENTIAL, distortion_type));
	root["distortion_type"]["choices"].append(add_property_choice_json("Full Wave Rectifier", FULL_WAVE_RECTIFIER, distortion_type));
	root["distortion_type"]["choices"].append(add_property_choice_json("Half Wave Rectifier", HALF_WAVE_RECTIFIER, distortion_type));

	// Return formatted string
	return root.toStyledString();
}
