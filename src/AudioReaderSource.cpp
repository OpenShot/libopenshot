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
      sample_position(0), speed(1), stream_position(0), requested_frame(starting_frame_number) {
}

// Destructor
AudioReaderSource::~AudioReaderSource()
{
}

// Get the next block of audio samples
void AudioReaderSource::getNextAudioBlock(const juce::AudioSourceChannelInfo& info)
{
	if (info.numSamples > 0) {
        info.clearActiveBufferRegion();
        const uint64_t generation = seek_generation.load();
        if (generation & 1)
            return;
        const int64_t position = requested_frame.load();
        if (seek_generation.load() != generation)
            return;
        if (applied_generation != generation) {
            frame_position = position;
            sample_position = 0;
            applied_generation = generation;
        }
        const uint64_t speed_epoch = speed_generation.load();
        // Commit the cursor only when this entire block is still current.
        // A pause during decoding must not consume samples that were silenced.
        int64_t next_frame = frame_position;
        int64_t next_sample = sample_position;
        std::shared_ptr<Frame> decoded_frame;
	    int remaining_samples = info.numSamples;
	    int remaining_position = info.startSample;

		// Pause and fill buffer with silence (wait for pre-roll)
		if (speed != 1 || (videoCache && !videoCache->isReady())) {
			return;
		}

        while (remaining_samples > 0) {
            const int previous_remaining = remaining_samples;
            decoded_frame.reset();
            try {
                // Get current frame object
                if (reader) {
                    decoded_frame = reader->GetFrame(next_frame);
                }
            }
            catch (const ReaderClosed & e) { }
            catch (const OutOfBoundsFrame & e) { }

            // A control request never waits for GetFrame. Discard decoding that
            // belongs to an older seek or playback state.
            if (seek_generation.load() != generation || speed_generation.load() != speed_epoch || speed.load() != 1)
                break;

            // Get audio samples
            if (reader && decoded_frame) {
                const int frame_samples = decoded_frame->GetAudioSamplesCount();
                const int frame_channels = decoded_frame->GetAudioChannelsCount();

                // Corrupt/unsupported streams can yield frames without audio data.
                // Avoid a tight loop that never consumes remaining_samples.
                if (frame_samples <= 0 || frame_channels <= 0) {
                    info.buffer->clear(remaining_position, remaining_samples);
                    break;
                }

                if (next_sample + remaining_samples <= decoded_frame->GetAudioSamplesCount()) {
                    // Success, we have enough samples
                    for (int channel = 0; channel < frame_channels; channel++) {
                        if (channel < info.buffer->getNumChannels()) {
                            info.buffer->copyFrom(channel, remaining_position, *decoded_frame->GetAudioSampleBuffer(),
                                                 channel, next_sample, remaining_samples);
                        }
                    }
                    next_sample += remaining_samples;
                    remaining_position += remaining_samples;
                    remaining_samples = 0;

                } else if (next_sample + remaining_samples > decoded_frame->GetAudioSamplesCount()) {
                    // Not enough samples, take what we can
                    int amount_to_copy = decoded_frame->GetAudioSamplesCount() - next_sample;
                    if (amount_to_copy <= 0) {
                        info.buffer->clear(remaining_position, remaining_samples);
                        break;
                    }

                    for (int channel = 0; channel < frame_channels; channel++) {
                        if (channel < info.buffer->getNumChannels()) {
                            info.buffer->copyFrom(channel, remaining_position, *decoded_frame->GetAudioSampleBuffer(), channel,
                                                 next_sample, amount_to_copy);
                        }
                    }
                    next_sample += amount_to_copy;
                    remaining_position += amount_to_copy;
                    remaining_samples -= amount_to_copy;
                }

                // Increment frame position (if samples are all used up)
                if (next_sample == decoded_frame->GetAudioSamplesCount()) {
                    ++next_frame;
                    next_sample = 0; // reset for new frame
                }
            } else {
                info.buffer->clear(remaining_position, remaining_samples);
                break;
            }

            if (remaining_samples == previous_remaining) {
                info.buffer->clear(remaining_position, remaining_samples);
                break;
            }
		}
        if (seek_generation.load() != generation || speed_generation.load() != speed_epoch || speed.load() != 1) {
            info.clearActiveBufferRegion();
            return;
        }
        frame_position = next_frame;
        sample_position = next_sample;
        std::atomic_store(&frame, decoded_frame);
        published_generation.store(generation);
	}
}

void AudioReaderSource::Seek(int64_t new_position)
{
    // Only control writers serialize. Cursor state belongs exclusively to the
    // callback; no decoding, transport or device lock is taken here.
    std::lock_guard<std::mutex> lock(seek_mutex);
    ++seek_generation;
    requested_frame.store(new_position);
    ++seek_generation;
}

std::shared_ptr<Frame> AudioReaderSource::getFrame() const
{
    const auto generation = seek_generation.load();
    auto result = std::atomic_load(&frame);
    if ((generation & 1) || published_generation.load() != generation ||
            seek_generation.load() != generation)
        return {};
    return result;
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
