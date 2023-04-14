/**
 * @file
 * @brief Source file for AudioWaveformer class
 * @author Jonathan Thomas <jonathan@openshot.org>
 *
 * @ref License
 */

// Copyright (c) 2008-2022 OpenShot Studios, LLC
//
// SPDX-License-Identifier: LGPL-3.0-or-later

#include "AudioWaveformer.h"


using namespace std;
using namespace openshot;


// Default constructor
AudioWaveformer::AudioWaveformer(ReaderBase* new_reader) : reader(new_reader)
{

}

// Destructor
AudioWaveformer::~AudioWaveformer()
{

}

// Extract audio samples from any ReaderBase class
AudioWaveformData AudioWaveformer::ExtractSamples(int channel, int num_per_second, bool normalize) {
    AudioWaveformData data;

    if (reader) {
        // Open reader (if needed)
        bool does_reader_have_video = reader->info.has_video;
        if (!reader->IsOpen()) {
            reader->Open();
        }
        // Disable video for faster processing
        reader->info.has_video = false;

        int sample_rate = reader->info.sample_rate;
        int sample_divisor = sample_rate / num_per_second;
        int total_samples = num_per_second * (reader->info.duration + 1.0);
        int extracted_index = 0;

        // Force output to zero elements for non-audio readers
        if (!reader->info.has_audio) {
            total_samples = 0;
        }

        // Resize and clear audio buffers
        data.resize(total_samples);
        data.zero(total_samples);

        // Bail out, if no samples needed
        if (total_samples == 0 || reader->info.channels == 0) {
            return data;
        }

        // Loop through all frames
        int sample_index = 0;
        float samples_max = 0.0;
        float chunk_max = 0.0;
        float chunk_squared_sum = 0.0;

        // How many channels are we using
        int channel_count = 1;
        if (channel == -1) {
            channel_count = reader->info.channels;
        }

        for (auto f = 1; f <= reader->info.video_length; f++) {
            // Get next frame
            shared_ptr<openshot::Frame> frame = reader->GetFrame(f);

            // Cache channels for this frame, to reduce # of calls to frame->GetAudioSamples
            float* channels[channel_count];
            for (auto channel_index = 0; channel_index < reader->info.channels; channel_index++) {
                if (channel == channel_index || channel == -1) {
                    channels[channel_index] = frame->GetAudioSamples(channel_index);
                }
            }

            // Get sample value from a specific channel (or all channels)
            for (auto s = 0; s < frame->GetAudioSamplesCount(); s++) {
                for (auto channel_index = 0; channel_index < reader->info.channels; channel_index++) {
                    if (channel == channel_index || channel == -1) {
                        float *samples = channels[channel_index];
                        float rms_sample_value = std::sqrt(samples[s] * samples[s]);

                        // Accumulate sample averages
                        chunk_squared_sum += rms_sample_value;
                        chunk_max = std::max(chunk_max, rms_sample_value);
                    }
                }

                sample_index += 1;

                // Cut-off reached
                if (sample_index % sample_divisor == 0) {
                    float avg_squared_sum = chunk_squared_sum / (sample_divisor * channel_count);
                    data.max_samples[extracted_index] = chunk_max;
                    data.rms_samples[extracted_index] = avg_squared_sum;
                    extracted_index++;

                    // Track max/min values
                    samples_max = std::max(samples_max, chunk_max);

                    // reset sample total and index
                    sample_index = 0;
                    chunk_max = 0.0;
                    chunk_squared_sum = 0.0;
                }
            }
        }

        // Scale all values to the -1 to +1 range (regardless of how small or how large the
        // original audio sample values are)
        if (normalize && samples_max > 0.0) {
            float scale = 1.0f / samples_max;
            data.scale(total_samples, scale);
        }

        // Resume previous has_video value
        reader->info.has_video = does_reader_have_video;
    }


    return data;
}
