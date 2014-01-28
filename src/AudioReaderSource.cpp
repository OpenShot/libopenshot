/**
 * @file
 * @brief Source file for AudioReaderSource class
 * @author Jonathan Thomas <jonathan@openshot.org>
 *
 * @section LICENSE
 *
 * Copyright (c) 2008-2013 OpenShot Studios, LLC
 * (http://www.openshotstudios.com). This file is part of
 * OpenShot Library (http://www.openshot.org), an open-source project
 * dedicated to delivering high quality video editing and animation solutions
 * to the world.
 *
 * OpenShot Library is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * OpenShot Library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with OpenShot Library.  If not, see <http://www.gnu.org/licenses/>.
 */

#include "../include/AudioReaderSource.h"

using namespace std;
using namespace openshot;

// Constructor that reads samples from a reader
AudioReaderSource::AudioReaderSource(ReaderBase *audio_reader, int64 starting_frame_number, int buffer_size)
	: reader(audio_reader), frame_number(starting_frame_number), size(buffer_size) {

	// Initialize an audio buffer (based on reader)
	buffer = new juce::AudioSampleBuffer(reader->info.channels, size);

	// initialize the audio samples to zero (silence)
	buffer->clear();
}

// Destructor
AudioReaderSource::~AudioReaderSource()
{
	// Clear and delete the buffer
	delete buffer;
	buffer = NULL;
};

// Get more samples from the reader
void AudioReaderSource::GetMoreSamplesFromReader() {
	// Determine the amount of samples needed to fill up this buffer
	int amount_needed = start; // replace these used samples
	int amount_remaining = size - start;
	if (!frame) {
		// If no frame, load entire buffer
		amount_needed = size;
		amount_remaining = size;
	}

	// Init new buffer
	juce::AudioSampleBuffer *new_buffer = new juce::AudioSampleBuffer(reader->info.channels, size);

	// Move the remaining samples into new buffer (if any)
	if (start > 0) {
		for (int channel = 0; channel < buffer->getNumChannels(); channel++)
			new_buffer->copyFrom(channel, 0, *buffer, channel, start, amount_remaining);
		start = amount_remaining;
	}

	// Loop through frames until buffer filled
	int fill_start = start;
	while (amount_needed > 0) {

		// Get the next frame
	    try {
	    	frame = reader->GetFrame(frame_number++);
	    } catch (const ReaderClosed & e) {
		break;
	    } catch (const TooManySeeks & e) {
		break;
	    } catch (const OutOfBoundsFrame & e) {
		break;
	    }

		// Is buffer big enough to hold entire frame
		if (fill_start + frame->GetAudioSamplesCount() > size) {
			// Increase size of buffer
			new_buffer->setSize(frame->GetAudioChannelsCount(), fill_start + frame->GetAudioSamplesCount(), true, true, false);
			size = new_buffer->getNumSamples();
		}

		// Load all of its samples into the buffer
		for (int channel = 0; channel < new_buffer->getNumChannels(); channel++)
			new_buffer->copyFrom(channel, fill_start, *frame->GetAudioSampleBuffer(), channel, 0, frame->GetAudioSamplesCount());

		// Adjust remaining samples
		amount_remaining -= fill_start + frame->GetAudioSamplesCount();
		fill_start += frame->GetAudioSamplesCount();
	}
}

// Get the next block of audio samples
void AudioReaderSource::getNextAudioBlock (const AudioSourceChannelInfo& info)
{
	int buffer_samples = buffer->getNumSamples();
	int buffer_channels = buffer->getNumChannels();

	if (info.numSamples > 0) {
		int start = position;
		int number_to_copy = 0;

		// Do we need more samples?
		if ((reader && reader->IsOpen() && !frame) or
		   (reader && reader->IsOpen() && buffer_samples - start < info.numSamples))
			// Refill buffer from reader
			GetMoreSamplesFromReader();

		// Determine how many samples to copy
		if (start + info.numSamples <= buffer_samples)
		{
			// copy the full amount requested
			number_to_copy = info.numSamples;
		}
		else if (start > buffer_samples)
		{
			// copy nothing
			number_to_copy = 0;
		}
		else if (buffer_samples - start > 0)
		{
			// only copy what is left in the buffer
			number_to_copy = buffer_samples - start;
		}
		else
		{
			// copy nothing
			number_to_copy = 0;
		}

		// Determine if any samples need to be copied
		if (number_to_copy > 0)
		{
			// Loop through each channel and copy some samples
			for (int channel = 0; channel < buffer_channels; channel++)
				info.buffer->copyFrom(channel, info.startSample, *buffer, channel, start, number_to_copy);

			// Update the position of this audio source
			position += number_to_copy;
		}

	}
}

// Prepare to play this audio source
void AudioReaderSource::prepareToPlay(int, double) { }

// Release all resources
void AudioReaderSource::releaseResources() { }

// Set the next read position of this source
void AudioReaderSource::setNextReadPosition (long long newPosition)
{
	// set position (if the new position is in range)
	if (newPosition >= 0 && newPosition < buffer->getNumSamples())
		position = newPosition;
}

// Get the next read position of this source
long long AudioReaderSource::getNextReadPosition() const
{
	// return the next read position
	return position;
}

// Get the total length (in samples) of this audio source
long long AudioReaderSource::getTotalLength() const
{
	// Get the length
	return buffer->getNumSamples();
}

// Determines if this audio source should repeat when it reaches the end
bool AudioReaderSource::isLooping() const
{
	// return if this source is looping
	return repeat;
}

// Set if this audio source should repeat when it reaches the end
void AudioReaderSource::setLooping (bool shouldLoop)
{
	// Set the repeat flag
	repeat = shouldLoop;
}

// Update the internal buffer used by this source
void AudioReaderSource::setBuffer (AudioSampleBuffer *audio_buffer)
{
	buffer = audio_buffer;
	setNextReadPosition(0);
}
