/**
 * @file
 * @brief Source file for Pitch audio effect class
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

#include "Pitch.h"
#include "Exceptions.h"

using namespace openshot;

/// Blank constructor, useful when using Json to load the effect properties
Pitch::Pitch() : shift(0),  fft_size(FFT_SIZE_32), hop_size(HOP_SIZE_2), window_type(BART_LETT){
	// Init effect properties
	init_effect_details();
}

// Default constructor
Pitch::Pitch(Keyframe new_shift, openshot::FFTSize new_fft_size, openshot::HopSize new_hop_size, openshot::WindowType new_window_type) : 
			 shift(new_shift), fft_size(new_fft_size), hop_size(new_hop_size), window_type(new_window_type)
{
	// Init effect properties
	init_effect_details();
}

// Init effect settings
void Pitch::init_effect_details()
{
	/// Initialize the values of the EffectInfo struct.
	InitEffectInfo();

	/// Set the effect info
	info.class_name = "Pitch";
	info.name = "Pitch Shift";
	info.description = "Change pitch of the frame's sound.";
	info.has_audio = true;
	info.has_video = false;
}

// This method is required for all derived classes of EffectBase, and returns a
// modified openshot::Frame object
std::shared_ptr<openshot::Frame> Pitch::GetFrame(std::shared_ptr<openshot::Frame> frame, int64_t frame_number)
{
	const ScopedLock sl (lock);
    ScopedNoDenormals noDenormals;

	updateFftSize(frame);
	updateHopSize();
	updateAnalysisWindow();
	updateWindowScaleFactor();

	// const ScopedLock sl (lock);
    // ScopedNoDenormals noDenormals;

	// copy of the AudioBuffer frame->audio object (not the pointer)
	// input_buffer = std::make_shared<juce::AudioBuffer<float>>(*frame->audio);
	// output_buffer = std::make_shared<juce::AudioBuffer<float>>(*frame->audio);
	// frame->audio;

    const int num_input_channels = frame->audio->getNumChannels();
    const int num_output_channels = frame->audio->getNumChannels();
    const int num_samples = frame->audio->getNumSamples();

    int current_input_buffer_write_position;
    int current_output_buffer_write_position;
    int current_output_buffer_read_position;
    int current_samples_since_last_FFT;

    float shift_value = powf(2.0f, shift.GetValue(frame_number) / 12.0f);
	int hop_size_value = 1 << ((int)hop_size + 1); 
	int fft_size_value = 1 << ((int)fft_size + 5); 

    float ratio = roundf(shift_value*(float)hop_size_value/(float)hop_size_value);
    int resampled_length = floorf((float)fft_size_value/ratio);
    juce::HeapBlock<float> resampled_output(resampled_length, true);
    juce::HeapBlock<float> synthesis_window(resampled_length, true);
    updateWindow(synthesis_window, resampled_length);

	for (int channel = 0; channel < num_input_channels; channel++)
	{
		float* channel_data = frame->audio->getWritePointer(channel);

        current_input_buffer_write_position = input_buffer_write_position;
        current_output_buffer_write_position = output_buffer_write_position;
        current_output_buffer_read_position = output_buffer_read_position;
        current_samples_since_last_FFT = samples_since_last_FFT;

        for (int sample = 0; sample < num_samples; ++sample) 
		{
            const float in = channel_data[sample];
            channel_data[sample] = output_buffer.getSample(channel, current_output_buffer_read_position);

			output_buffer.setSample(channel, current_output_buffer_read_position, 0.0f);
			if (++current_output_buffer_read_position >= output_buffer_length)
                current_output_buffer_read_position = 0;
            
			input_buffer.setSample(channel, current_input_buffer_write_position, in);
            if (++current_input_buffer_write_position >= input_buffer_length)
                current_input_buffer_write_position = 0;

			if (++current_samples_since_last_FFT >= hop_size_value) 
			{
                current_samples_since_last_FFT = 0;

				int input_buffer_index = current_input_buffer_write_position;

                for (int index = 0; index < fft_size_value; ++index) {
                    fft_time_domain[index].real(sqrtf(fft_window[index]) * input_buffer.getSample(channel, input_buffer_index));
                    fft_time_domain[index].imag(0.0f);

                    if (++input_buffer_index >= input_buffer_length)
                        input_buffer_index = 0;
                }

				fft->perform(fft_time_domain, fft_frequency_domain, false);
				
				/*
                if (paramShift.isSmoothing())
                    needToResetPhases = true;
                if (shift == paramShift.getTargetValue() && needToResetPhases) {
                    inputPhase.clear();
                    outputPhase.clear();
                    needToResetPhases = false;
                }
				*/

				for (int index = 0; index < fft_size_value; ++index) {
                    float magnitude = abs(fft_frequency_domain[index]);
                    float phase = arg(fft_frequency_domain[index]);

                    float phase_deviation = phase - input_phase.getSample(channel, index) - omega[index] * (float)hop_size_value;
                    float delta_phi = omega[index] * hop_size_value + princArg(phase_deviation);
                    float new_phase = princArg(output_phase.getSample(channel, index) + delta_phi * ratio);

                    input_phase.setSample(channel, index, phase);
                    output_phase.setSample(channel, index, new_phase);
                    fft_frequency_domain[index] = std::polar(magnitude, new_phase);
                }

                fft->perform(fft_frequency_domain, fft_time_domain, true);

				for (int index = 0; index < resampled_length; ++index) {
                    float x = (float)index * (float)fft_size_value / (float)resampled_length;
                    int ix = (int)floorf(x);
                    float dx = x - (float)ix;

                    float sample1 = fft_time_domain[ix].real();
                    float sample2 = fft_time_domain[(ix + 1) % fft_size_value].real();
                    resampled_output[index] = sample1 + dx * (sample2 - sample1);
                    resampled_output[index] *= sqrtf(synthesis_window[index]);
                }

				int output_buffer_index = current_output_buffer_write_position;

                for (int index = 0; index < resampled_length; ++index) {
                    float out = output_buffer.getSample(channel, output_buffer_index);
                    out += resampled_output[index] * window_scale_factor;
                    output_buffer.setSample(channel, output_buffer_index, out);

                    if (++output_buffer_index >= output_buffer_length)
                        output_buffer_index = 0;
                }

                current_output_buffer_write_position += hop_size_value;
                if (current_output_buffer_write_position >= output_buffer_length)
                    current_output_buffer_write_position = 0;
			}

		}
	}
	
	input_buffer_write_position = current_input_buffer_write_position;
    output_buffer_write_position = current_output_buffer_write_position;
    current_output_buffer_read_position = current_output_buffer_read_position;
    samples_since_last_FFT = current_samples_since_last_FFT;

    for (int channel = num_input_channels; channel < num_output_channels; ++channel)
        frame->audio->clear(channel, 0, num_samples);

	// frame->audio = std::make_shared<juce::AudioBuffer<float>>(output_buffer);

	// return the modified frame
	return frame;
}

void Pitch::updateFftSize(std::shared_ptr<openshot::Frame> frame)
{
	int fft_size_value = 1 << ((int)fft_size + 5); 
    fft = std::make_unique<juce::dsp::FFT>(log2(fft_size_value));

    input_buffer_length = fft_size_value;
    input_buffer_write_position = 0;
    input_buffer.clear();
    input_buffer.setSize(frame->audio->getNumChannels(), input_buffer_length);

    float max_ratio = powf(2.0f, -12/12.0f);

    output_buffer_length = (int)floorf ((float)fft_size_value / max_ratio);

    output_buffer_write_position = 0;
    output_buffer_read_position = 0;
    output_buffer.clear();
    output_buffer.setSize(frame->audio->getNumChannels(), output_buffer_length);

    fft_window.realloc(fft_size_value);
    fft_window.clear(fft_size_value);

    fft_time_domain.realloc(fft_size_value);
    fft_time_domain.clear(fft_size_value);

    fft_frequency_domain.realloc(fft_size_value);
    fft_frequency_domain.clear(fft_size_value);

    samples_since_last_FFT = 0;

    //======================================

    omega.realloc(fft_size_value);

    for (int index = 0; index < fft_size_value; ++index)
        omega[index] = 2.0f * M_PI * index / (float)fft_size_value;

    input_phase.clear();
    input_phase.setSize(frame->audio->getNumChannels(), output_buffer_length);

    output_phase.clear();
    output_phase.setSize(frame->audio->getNumChannels(), output_buffer_length);
}


void Pitch::updateHopSize()
{
	int hop_size_value = 1 << ((int)hop_size + 1); 
	int fft_size_value = 1 << ((int)fft_size + 5); 
    overlap = hop_size_value;

    if (overlap != 0) {
        hop_size_value = fft_size_value / overlap;
		// hop_size = hop_size_value;
        output_buffer_write_position = hop_size_value % output_buffer_length;
    }
}


void Pitch::updateWindowScaleFactor()
{
	int fft_size_value = 1 << ((int)fft_size + 5);
    float window_sum = 0.0f;

    for (int sample = 0; sample < fft_size_value; ++sample)
        window_sum += fft_window[sample];

    window_scale_factor = 0.0f;

    if (overlap != 0 && window_sum != 0.0f)
        window_scale_factor = 1.0f / (float)overlap / window_sum * (float)fft_size_value;
}


void Pitch::updateAnalysisWindow()
{
	int fft_size_value = 1 << ((int)fft_size + 5); 
    updateWindow(fft_window, fft_size_value);
}

void Pitch::updateWindow(const juce::HeapBlock<float> &window, const int window_length)
{
    switch ((int)window_type) {
        case BART_LETT: {
            for (int sample = 0; sample < window_length; ++sample)
                window[sample] = 1.0f - fabs(2.0f * (float)sample / (float)(window_length - 1) - 1.0f);
            break;
        }
        case HANN: {
            for (int sample = 0; sample < window_length; ++sample)
                window[sample] = 0.5f - 0.5f * cosf(2.0f * M_PI * (float)sample / (float)(window_length - 1));
            break;
        }
        case HAMMING: {
            for (int sample = 0; sample < window_length; ++sample)
                window[sample] = 0.54f - 0.46f * cosf(2.0f * M_PI * (float)sample / (float)(window_length - 1));
            break;
        }
    }
}

float Pitch::princArg(const float phase)
{
    if (phase >= 0.0f)
        return fmod(phase + M_PI,  2.0f * M_PI) - M_PI;
    else
        return fmod(phase + M_PI, -2.0f * M_PI) + M_PI;
}

// Generate JSON string of this object
std::string Pitch::Json() const {

	// Return formatted string
	return JsonValue().toStyledString();
}

// Generate Json::Value for this object
Json::Value Pitch::JsonValue() const {

	// Create root json object
	Json::Value root = EffectBase::JsonValue(); // get parent properties
	root["type"] = info.class_name;
	root["shift"] = shift.JsonValue();
	root["fft_size"] = fft_size;
	root["hop_size"] = hop_size;
	root["window_type"] = window_type;

	// return JsonValue
	return root;
}

// Load JSON string into this object
void Pitch::SetJson(const std::string value) {

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
void Pitch::SetJsonValue(const Json::Value root) {

	// Set parent data
	EffectBase::SetJsonValue(root);

	if (!root["fft_size"].isNull())
		fft_size = (FFTSize)root["fft_size"].asInt();

	if (!root["hop_size"].isNull())
		hop_size = (HopSize)root["hop_size"].asInt();

	if (!root["window_type"].isNull())
		window_type = (WindowType)root["window_type"].asInt();

	// Set data from Json (if key is found)
	if (!root["shift"].isNull())
		shift.SetJsonValue(root["shift"]);
}

// Get all properties for a specific frame
std::string Pitch::PropertiesJSON(int64_t requested_frame) const {

	// Generate JSON properties list
	Json::Value root;
	root["id"] = add_property_json("ID", 0.0, "string", Id(), NULL, -1, -1, true, requested_frame);
	root["layer"] = add_property_json("Track", Layer(), "int", "", NULL, 0, 20, false, requested_frame);
	root["start"] = add_property_json("Start", Start(), "float", "", NULL, 0, 1000 * 60 * 30, false, requested_frame);
	root["end"] = add_property_json("End", End(), "float", "", NULL, 0, 1000 * 60 * 30, false, requested_frame);
	root["duration"] = add_property_json("Duration", Duration(), "float", "", NULL, 0, 1000 * 60 * 30, true, requested_frame);

	// Keyframes
	root["shift"] = add_property_json("Shift", shift.GetValue(requested_frame), "float", "", &shift, -12, 12, false, requested_frame);
	root["fft_size"] = add_property_json("FFT Size", fft_size, "int", "", NULL, 0, 8, false, requested_frame);
	root["hop_size"] = add_property_json("Hop Size", hop_size, "int", "", NULL, 0, 2, false, requested_frame);
	root["window_type"] = add_property_json("Window Type", window_type, "int", "", NULL, 0, 2, false, requested_frame);

	// Add fft_size choices (dropdown style)
	root["fft_size"]["choices"].append(add_property_choice_json("32", FFT_SIZE_32, fft_size));
	root["fft_size"]["choices"].append(add_property_choice_json("64", FFT_SIZE_64, fft_size));
	root["fft_size"]["choices"].append(add_property_choice_json("128", FFT_SIZE_128, fft_size));
	root["fft_size"]["choices"].append(add_property_choice_json("256", FFT_SIZE_256, fft_size));
	root["fft_size"]["choices"].append(add_property_choice_json("512", FFT_SIZE_512, fft_size));
	root["fft_size"]["choices"].append(add_property_choice_json("1024", FFT_SIZE_1024, fft_size));
	root["fft_size"]["choices"].append(add_property_choice_json("2048", FFT_SIZE_2048, fft_size));
	root["fft_size"]["choices"].append(add_property_choice_json("4096", FFT_SIZE_4096, fft_size));
	root["fft_size"]["choices"].append(add_property_choice_json("8192", FFT_SIZE_8192, fft_size));
	
	// Add hop_size choices (dropdown style)
	root["hop_size"]["choices"].append(add_property_choice_json("2", HOP_SIZE_2, hop_size));
	root["hop_size"]["choices"].append(add_property_choice_json("4", HOP_SIZE_4, hop_size));
	root["hop_size"]["choices"].append(add_property_choice_json("8", HOP_SIZE_8, hop_size));

	// Add window_type choices (dropdown style)
	root["window_type"]["choices"].append(add_property_choice_json("Bart Lett", BART_LETT, window_type));
	root["window_type"]["choices"].append(add_property_choice_json("Hann", HANN, window_type));
	root["window_type"]["choices"].append(add_property_choice_json("Hamming", HAMMING, window_type));

	// Return formatted string
	return root.toStyledString();
}
