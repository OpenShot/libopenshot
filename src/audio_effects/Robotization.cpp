/**
 * @file
 * @brief Source file for Robotization audio effect class
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

#include "Robotization.h"
#include "Exceptions.h"

using namespace openshot;


/// Blank constructor, useful when using Json to load the effect properties
Robotization::Robotization() : fft_size(FFT_SIZE_512), hop_size(HOP_SIZE_2), window_type(RECTANGULAR), stft(*this) {
	// Init effect properties
	init_effect_details();
}

// Default constructor
Robotization::Robotization(openshot::FFTSize new_fft_size, openshot::HopSize new_hop_size, openshot::WindowType new_window_type) : 
			 fft_size(new_fft_size), hop_size(new_hop_size), window_type(new_window_type), stft(*this)
{
	// Init effect properties
	init_effect_details();
}

// Init effect settings
void Robotization::init_effect_details()
{
	/// Initialize the values of the EffectInfo struct.
	InitEffectInfo();

	/// Set the effect info
	info.class_name = "Robotization";
	info.name = "Robotization";
	info.description = "Add robotization effect on the frame's audio track. This effect transforms the voice present in an audio into a robotic voice effect.";
	info.has_audio = true;
	info.has_video = false;
}

// This method is required for all derived classes of EffectBase, and returns a
// modified openshot::Frame object
std::shared_ptr<openshot::Frame> Robotization::GetFrame(std::shared_ptr<openshot::Frame> frame, int64_t frame_number)
{
	const ScopedLock sl (lock);
    ScopedNoDenormals noDenormals;

    const int num_input_channels = frame->audio->getNumChannels();
    const int num_output_channels = frame->audio->getNumChannels();
    const int num_samples = frame->audio->getNumSamples();
    const int hop_size_value = 1 << ((int)hop_size + 1); 
	const int fft_size_value = 1 << ((int)fft_size + 5); 

    stft.setup(num_output_channels);
    stft.updateParameters((int)fft_size_value,
                          (int)hop_size_value,
                          (int)window_type);

    stft.process(*frame->audio);

	// return the modified frame
	return frame;
}

void Robotization::RobotizationEffect::modification(const int channel)
{
	fft->perform(time_domain_buffer, frequency_domain_buffer, false);

	for (int index = 0; index < fft_size; ++index) {
		float magnitude = abs(frequency_domain_buffer[index]);
		frequency_domain_buffer[index].real(magnitude);
		frequency_domain_buffer[index].imag(0.0f);
	}

	fft->perform(frequency_domain_buffer, time_domain_buffer, true);
}

// Generate JSON string of this object
std::string Robotization::Json() const {

	// Return formatted string
	return JsonValue().toStyledString();
}

// Generate Json::Value for this object
Json::Value Robotization::JsonValue() const {

	// Create root json object
	Json::Value root = EffectBase::JsonValue(); // get parent properties
	root["type"] = info.class_name;
	root["fft_size"] = fft_size;
	root["hop_size"] = hop_size;
	root["window_type"] = window_type;

	// return JsonValue
	return root;
}

// Load JSON string into this object
void Robotization::SetJson(const std::string value) {

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
void Robotization::SetJsonValue(const Json::Value root) {

	// Set parent data
	EffectBase::SetJsonValue(root);

	if (!root["fft_size"].isNull())
		fft_size = (FFTSize)root["fft_size"].asInt();

	if (!root["hop_size"].isNull())
		hop_size = (HopSize)root["hop_size"].asInt();

	if (!root["window_type"].isNull())
		window_type = (WindowType)root["window_type"].asInt();
}

// Get all properties for a specific frame
std::string Robotization::PropertiesJSON(int64_t requested_frame) const {

	// Generate JSON properties list
	Json::Value root;
	root["id"] = add_property_json("ID", 0.0, "string", Id(), NULL, -1, -1, true, requested_frame);
	root["layer"] = add_property_json("Track", Layer(), "int", "", NULL, 0, 20, false, requested_frame);
	root["start"] = add_property_json("Start", Start(), "float", "", NULL, 0, 1000 * 60 * 30, false, requested_frame);
	root["end"] = add_property_json("End", End(), "float", "", NULL, 0, 1000 * 60 * 30, false, requested_frame);
	root["duration"] = add_property_json("Duration", Duration(), "float", "", NULL, 0, 1000 * 60 * 30, true, requested_frame);

	// Keyframes
	root["fft_size"] = add_property_json("FFT Size", fft_size, "int", "", NULL, 0, 8, false, requested_frame);
	root["hop_size"] = add_property_json("Hop Size", hop_size, "int", "", NULL, 0, 2, false, requested_frame);
	root["window_type"] = add_property_json("Window Type", window_type, "int", "", NULL, 0, 3, false, requested_frame);

	// Add fft_size choices (dropdown style)
	root["fft_size"]["choices"].append(add_property_choice_json("128", FFT_SIZE_128, fft_size));
	root["fft_size"]["choices"].append(add_property_choice_json("256", FFT_SIZE_256, fft_size));
	root["fft_size"]["choices"].append(add_property_choice_json("512", FFT_SIZE_512, fft_size));
	root["fft_size"]["choices"].append(add_property_choice_json("1024", FFT_SIZE_1024, fft_size));
	root["fft_size"]["choices"].append(add_property_choice_json("2048", FFT_SIZE_2048, fft_size));
	
	// Add hop_size choices (dropdown style)
	root["hop_size"]["choices"].append(add_property_choice_json("1/2", HOP_SIZE_2, hop_size));
	root["hop_size"]["choices"].append(add_property_choice_json("1/4", HOP_SIZE_4, hop_size));
	root["hop_size"]["choices"].append(add_property_choice_json("1/8", HOP_SIZE_8, hop_size));

	// Add window_type choices (dropdown style)
	root["window_type"]["choices"].append(add_property_choice_json("Rectangular", RECTANGULAR, window_type));
	root["window_type"]["choices"].append(add_property_choice_json("Bart Lett", BART_LETT, window_type));
	root["window_type"]["choices"].append(add_property_choice_json("Hann", HANN, window_type));
	root["window_type"]["choices"].append(add_property_choice_json("Hamming", HAMMING, window_type));
	
	// Return formatted string
	return root.toStyledString();
}