/**
 * @file
 * @brief Source file for AudioResampler class
 * @author Jonathan Thomas <jonathan@openshot.org>
 *
 * @ref License
 */

// Copyright (c) 2008-2019 OpenShot Studios, LLC
//
// SPDX-License-Identifier: LGPL-3.0-or-later

#include "AudioResampler.h"

using namespace std;
using namespace openshot;

// Default constructor, max frames to cache is 20 // resample_source(NULL), buffer_source(NULL), num_of_samples(0), new_num_of_samples(0), dest_ratio(0), source_ratio(0), resampled_buffer(NULL), isPrepared(false)
AudioResampler::AudioResampler(int numChannels)
{
	buffer = NULL;
	resample_source = NULL;
	buffer_source = NULL;
	num_of_samples = 0;
	new_num_of_samples = 0;
	dest_ratio = 0;
	source_ratio = 0;
	resampled_buffer = NULL;
	isPrepared = false;

	// Init buffer source
	buffer_source = new AudioBufferSource(buffer);

	// Init resampling source
	resample_source = new juce::ResamplingAudioSource(buffer_source, false, numChannels);

	// Init resampled buffer
	resampled_buffer = new juce::AudioBuffer<float>(2, 1);
	resampled_buffer->clear();

	// Init callback buffer
	resample_callback_buffer.buffer = resampled_buffer;
	resample_callback_buffer.numSamples = 1;
	resample_callback_buffer.startSample = 0;
}

// Descructor
AudioResampler::~AudioResampler()
{
	// Clean up
	if (buffer_source)
		delete buffer_source;
	if (resample_source)
		delete resample_source;
	if (resampled_buffer)
		delete resampled_buffer;
}

// Sets the audio buffer and updates the key settings
void AudioResampler::SetBuffer(juce::AudioBuffer<float> *new_buffer, double sample_rate, double new_sample_rate)
{
	if (sample_rate <= 0)
		sample_rate = 44100;
	if (new_sample_rate <= 0)
		new_sample_rate = 44100;

	// Set the sample ratio (the ratio of sample rate change)
	source_ratio = sample_rate / new_sample_rate;

	// Call SetBuffer with ratio
	SetBuffer(new_buffer, source_ratio);
}

// Sets the audio buffer and key settings
void AudioResampler::SetBuffer(juce::AudioBuffer<float> *new_buffer, double ratio)
{
	// Update buffer & buffer source
	buffer = new_buffer;
	buffer_source->setBuffer(buffer);

	// Set the sample ratio (the ratio of sample rate change)
	source_ratio = ratio;
	dest_ratio = 1.0 / ratio;
	num_of_samples = buffer->getNumSamples();
	new_num_of_samples = round(num_of_samples * dest_ratio);

	// Set resample ratio
	resample_source->setResamplingRatio(source_ratio);

	// Prepare to play resample source
	if (!isPrepared)
	{
		// Prepare to play the audio sources (and set the # of samples per chunk to a little more than expected)
		resample_source->prepareToPlay(num_of_samples + 10, 0);
		isPrepared = true;
	}

	// Resize buffer for the newly resampled data
	resampled_buffer->setSize(buffer->getNumChannels(), new_num_of_samples, true, true, true);
	resample_callback_buffer.numSamples = new_num_of_samples;
	resample_callback_buffer.startSample = 0;
	resample_callback_buffer.clearActiveBufferRegion();
}

// Get the resampled audio buffer
juce::AudioBuffer<float>* AudioResampler::GetResampledBuffer()
{
	// Resample the current frame's audio buffer (into the temp callback buffer)
	resample_source->getNextAudioBlock(resample_callback_buffer);

	// Return buffer pointer to this newly resampled buffer
	return resampled_buffer;
}
