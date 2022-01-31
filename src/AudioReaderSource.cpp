/**
 * @file
 * @brief Source file for AudioReaderSource class
 * @author Jonathan Thomas <jonathan@openshot.org>
 *
 * @ref License
 */

// Copyright (c) 2008-2019 OpenShot Studios, LLC
//
// SPDX-License-Identifier: LGPL-3.0-or-later

#include "AudioReaderSource.h"
#include "Exceptions.h"
#include "Frame.h"


using namespace std;
using namespace openshot;

// Constructor that reads samples from a reader
AudioReaderSource::AudioReaderSource(ReaderBase *audio_reader, int64_t starting_frame_number)
	: reader(audio_reader), frame_position(starting_frame_number), videoCache(NULL), frame(NULL),
      sample_position(0), speed(1), stream_position(0) {
}

// Destructor
AudioReaderSource::~AudioReaderSource()
{
}

// Get the next block of audio samples
void AudioReaderSource::getNextAudioBlock(const juce::AudioSourceChannelInfo& info)
{
	if (info.numSamples > 0) {
	    int remaining_samples = info.numSamples;
	    int remaining_position = info.startSample;

		// Pause and fill buffer with silence (wait for pre-roll)
		if (speed != 1 || !videoCache->isReady()) {
			info.buffer->clear();
			return;
		}

        while (remaining_samples > 0) {

            try {
                // Get current frame object
                if (reader) {
                    frame = reader->GetFrame(frame_position);
                }
            }
            catch (const ReaderClosed & e) { }
            catch (const OutOfBoundsFrame & e) { }

            // Get audio samples
            if (reader && frame) {
                if (sample_position + remaining_samples <= frame->GetAudioSamplesCount()) {
                    // Success, we have enough samples
                    for (int channel = 0; channel < frame->GetAudioChannelsCount(); channel++) {
                        if (channel < info.buffer->getNumChannels()) {
                            info.buffer->addFrom(channel, remaining_position, *frame->GetAudioSampleBuffer(),
                                                 channel, sample_position, remaining_samples);
                        }
                    }
                    sample_position += remaining_samples;
                    remaining_position += remaining_samples;
                    remaining_samples = 0;

                } else if (sample_position + remaining_samples > frame->GetAudioSamplesCount()) {
                    // Not enough samples, take what we can
                    int amount_to_copy = frame->GetAudioSamplesCount() - sample_position;
                    for (int channel = 0; channel < frame->GetAudioChannelsCount(); channel++) {
                        if (channel < info.buffer->getNumChannels()) {
                            info.buffer->addFrom(channel, remaining_position, *frame->GetAudioSampleBuffer(), channel,
                                                 sample_position, amount_to_copy);
                        }
                    }
                    sample_position += amount_to_copy;
                    remaining_position += amount_to_copy;
                    remaining_samples -= amount_to_copy;
                }

                // Increment frame position (if samples are all used up)
                if (sample_position == frame->GetAudioSamplesCount()) {
                    frame_position += speed;
                    sample_position = 0; // reset for new frame
                }

            }
		}
	}
}

// Prepare to play this audio source
void AudioReaderSource::prepareToPlay(int, double) {}

// Release all resources
void AudioReaderSource::releaseResources() { }

// Get the total length (in samples) of this audio source
juce::int64 AudioReaderSource::getTotalLength() const
{
	// Get the length
	if (reader)
		return reader->info.sample_rate * reader->info.duration;
	else
		return 0;
}
