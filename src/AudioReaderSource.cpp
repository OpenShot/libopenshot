/**
 * @file
 * @brief Source file for AudioReaderSource class
 * @author Jonathan Thomas <jonathan@openshot.org>
 *
 * @section LICENSE
 *
 * Copyright (c) 2008-2014 OpenShot Studios, LLC
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

#include "../include/AudioReaderSource.h"

using namespace std;
using namespace openshot;

// Constructor that reads samples from a reader
AudioReaderSource::AudioReaderSource(ReaderBase *audio_reader, int64 starting_frame_number, int buffer_size)
	: reader(audio_reader), frame_number(starting_frame_number), original_frame_number(starting_frame_number),
	  size(buffer_size), position(0), frame_position(0), estimated_frame(0), speed(1) {

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
void AudioReaderSource::GetMoreSamplesFromReader()
{
	// Determine the amount of samples needed to fill up this buffer
	int amount_needed = position; // replace these used samples
	int amount_remaining = size - amount_needed; // these are unused samples, and need to be carried forward
	if (!frame) {
		// If no frame, load entire buffer
		amount_needed = size;
		amount_remaining = 0;
	}

	// Debug
	ZmqLogger::Instance()->AppendDebugMethod("AudioReaderSource::GetMoreSamplesFromReader", "amount_needed", amount_needed, "amount_remaining", amount_remaining, "", -1, "", -1, "", -1, "", -1);

	// Init estimated buffer equal to the current frame position (before getting more samples)
	estimated_frame = frame_number;

	// Init new buffer
	juce::AudioSampleBuffer *new_buffer = new juce::AudioSampleBuffer(reader->info.channels, size);
	new_buffer->clear();

	// Move the remaining samples into new buffer (if any)
	if (amount_remaining > 0) {
		for (int channel = 0; channel < buffer->getNumChannels(); channel++)
		    new_buffer->addFrom(channel, 0, *buffer, channel, position, amount_remaining);

		position = amount_remaining;
	} else
		// reset position to 0
		position = 0;

	// Loop through frames until buffer filled
	while (amount_needed > 0 && speed == 1  && frame_number >= 1 && frame_number <= reader->info.video_length) {

		// Get the next frame (if position is zero)
		if (frame_position == 0) {
			try {
				// Get frame object
				frame = reader->GetFrame(frame_number);
				frame_number = frame_number + speed;

			} catch (const ReaderClosed & e) {
			break;
			} catch (const TooManySeeks & e) {
			break;
			} catch (const OutOfBoundsFrame & e) {
			break;
			}
		}

		bool frame_completed = false;
		int amount_to_copy = 0;
		if (frame)
			amount_to_copy = frame->GetAudioSamplesCount() - frame_position;
		if (amount_to_copy > amount_needed) {
			// Don't copy too many samples (we don't want to overflow the buffer)
			amount_to_copy = amount_needed;
			amount_needed = 0;
		} else {
			// Not enough to fill the buffer (so use the entire frame)
			amount_needed -= amount_to_copy;
			frame_completed = true;
		}

		// Load all of its samples into the buffer
		if (frame)
			for (int channel = 0; channel < new_buffer->getNumChannels(); channel++)
				new_buffer->addFrom(channel, position, *frame->GetAudioSampleBuffer(), channel, frame_position, amount_to_copy);

		// Adjust remaining samples
		position += amount_to_copy;
		if (frame_completed)
			// Reset frame buffer position (which will load a new frame on the next loop)
			frame_position = 0;
		else
			// Continue tracking the current frame's position
			frame_position += amount_to_copy;
	}

	// Delete old buffer
	buffer->clear();
	delete buffer;

	// Replace buffer and reset position
	buffer = new_buffer;
	position = 0;
}

// Reverse an audio buffer
juce::AudioSampleBuffer* AudioReaderSource::reverse_buffer(juce::AudioSampleBuffer* buffer)
{
	int number_of_samples = buffer->getNumSamples();
	int channels = buffer->getNumChannels();

	// Debug
	ZmqLogger::Instance()->AppendDebugMethod("AudioReaderSource::reverse_buffer", "number_of_samples", number_of_samples, "channels", channels, "", -1, "", -1, "", -1, "", -1);

	// Reverse array (create new buffer to hold the reversed version)
	AudioSampleBuffer *reversed = new juce::AudioSampleBuffer(channels, number_of_samples);
	reversed->clear();

	for (int channel = 0; channel < channels; channel++)
	{
		int n=0;
		for (int s = number_of_samples - 1; s >= 0; s--, n++)
			reversed->getWritePointer(channel)[n] = buffer->getWritePointer(channel)[s];
	}

	// Copy the samples back to the original array
	buffer->clear();
	// Loop through channels, and get audio samples
	for (int channel = 0; channel < channels; channel++)
		// Get the audio samples for this channel
		buffer->addFrom(channel, 0, reversed->getReadPointer(channel), number_of_samples, 1.0f);

	delete reversed;
	reversed = NULL;

	// return pointer or passed in object (so this method can be chained together)
	return buffer;
}

// Get the next block of audio samples
void AudioReaderSource::getNextAudioBlock(const AudioSourceChannelInfo& info)
{
	int buffer_samples = buffer->getNumSamples();
	int buffer_channels = buffer->getNumChannels();

	if (info.numSamples > 0) {
		int number_to_copy = 0;

		// Do we need more samples?
		if (speed == 1) {
			// Only refill buffers if speed is normal
			if ((reader && reader->IsOpen() && !frame) or
				(reader && reader->IsOpen() && buffer_samples - position < info.numSamples))
				// Refill buffer from reader
				GetMoreSamplesFromReader();
		} else {
			// Fill buffer with silence and clear current frame
			info.buffer->clear();
			return;
		}

		// Determine how many samples to copy
		if (position + info.numSamples <= buffer_samples)
		{
			// copy the full amount requested
			number_to_copy = info.numSamples;
		}
		else if (position > buffer_samples)
		{
			// copy nothing
			number_to_copy = 0;
		}
		else if (buffer_samples - position > 0)
		{
			// only copy what is left in the buffer
			number_to_copy = buffer_samples - position;
		}
		else
		{
			// copy nothing
			number_to_copy = 0;
		}


		// Determine if any samples need to be copied
		if (number_to_copy > 0)
		{
			// Debug
			ZmqLogger::Instance()->AppendDebugMethod("AudioReaderSource::getNextAudioBlock", "number_to_copy", number_to_copy, "buffer_samples", buffer_samples, "buffer_channels", buffer_channels, "info.numSamples", info.numSamples, "speed", speed, "position", position);

			// Loop through each channel and copy some samples
			for (int channel = 0; channel < buffer_channels; channel++)
				info.buffer->copyFrom(channel, info.startSample, *buffer, channel, position, number_to_copy);

			// Update the position of this audio source
			position += number_to_copy;
		}

		// Adjust estimate frame number (the estimated frame number that is being played)
		estimated_samples_per_frame = Frame::GetSamplesPerFrame(estimated_frame, reader->info.fps, reader->info.sample_rate, buffer_channels);
		estimated_frame += double(info.numSamples) / double(estimated_samples_per_frame);
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
	if (reader)
		return reader->info.sample_rate * reader->info.duration;
	else
		return 0;
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
