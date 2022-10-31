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
std::vector<float> AudioWaveformer::ExtractSamples(int channel, int num_per_second, bool normalize) {
    std::vector<float> extracted_data(0);

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

        // Size audio buffer (for smaller dataset)
        extracted_data.resize(total_samples);
        int extracted_index = 0;

        // Clear audio buffer
        for (auto s = 0; s < total_samples; s++) {
            extracted_data[s] = 0.0;
        }

        // Loop through all frames
        int sample_index = 0;
        float samples_total = 0.0;
        float samples_max = 0.0;
        float samples_min = 0.0;

        for (auto f = 1; f <= reader->info.video_length; f++) {
            // Get next frame
            shared_ptr<openshot::Frame> frame = reader->GetFrame(f);

            float* samples = frame->GetAudioSamples(channel);
            for (auto s = 0; s < frame->GetAudioSamplesCount(); s++) {
                samples_total += samples[s];
                sample_index += 1;

                // Cut-off reached
                if (sample_index % sample_divisor == 0) {
                    float avg_sample_value = samples_total / sample_divisor;
                    extracted_data[extracted_index] = avg_sample_value;
                    extracted_index++;

                    // Track max/min values
                    samples_max = std::max(samples_max, avg_sample_value);
                    samples_min = std::min(samples_min, avg_sample_value);

                    // reset sample total and index
                    sample_index = 0;
                    samples_total = 0.0;
                }
            }
        }

        // Scale all values to the -1 to +1 range (regardless of how small or how large the
        // original audio sample values are)
        if (normalize) {
            float scale = std::min(1.0f / samples_max, 1.0f / std::fabs(samples_min));
            for (auto s = 0; s < total_samples; s++) {
                extracted_data[s] *= scale;
            }
        }

        // Resume previous has_video value
        reader->info.has_video = does_reader_have_video;
    }

    return extracted_data;
}
