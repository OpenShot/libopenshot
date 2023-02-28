/**
 * @file
 * @brief Header file for AudioResampler class
 * @author Jonathan Thomas <jonathan@openshot.org>
 *
 * @ref License
 */

// Copyright (c) 2008-2019 OpenShot Studios, LLC
//
// SPDX-License-Identifier: LGPL-3.0-or-later

#ifndef OPENSHOT_RESAMPLER_H
#define OPENSHOT_RESAMPLER_H

#include "AudioBufferSource.h"

#include <AppConfig.h>
#include <juce_audio_basics/juce_audio_basics.h>
#include <juce_audio_devices/juce_audio_devices.h>

namespace openshot {

	/**
	 * @brief This class is used to resample audio data for many sequential frames.
	 *
	 * It maintains some data from the last call to GetResampledBuffer(), so there
	 * are no pops and clicks between frames.
	 */
	class AudioResampler {
	private:
		juce::AudioBuffer<float> *buffer;
		juce::AudioBuffer<float> *resampled_buffer;
		openshot::AudioBufferSource *buffer_source;
		juce::ResamplingAudioSource *resample_source;
		juce::AudioSourceChannelInfo resample_callback_buffer;

		int num_of_samples;
		int new_num_of_samples;
		double dest_ratio;
		double source_ratio;
		bool isPrepared;

	public:
		/// Default constructor
		AudioResampler(int numChannels=2);

		/// Destructor
		~AudioResampler();

		/// @brief Sets the audio buffer and key settings
		/// @param new_buffer The buffer of audio samples needing to be resampled
		/// @param sample_rate The original sample rate of the buffered samples
		/// @param new_sample_rate The requested sample rate you need
		void SetBuffer(juce::AudioBuffer<float> *new_buffer, double sample_rate, double new_sample_rate);

		/// @brief Sets the audio buffer and key settings
		/// @param new_buffer The buffer of audio samples needing to be resampled
		/// @param ratio The multiplier that needs to be applied to the sample rate (this is how resampling happens)
		void SetBuffer(juce::AudioBuffer<float> *new_buffer, double ratio);

		/// Get the resampled audio buffer
		juce::AudioBuffer<float>* GetResampledBuffer();
	};

}

#endif
