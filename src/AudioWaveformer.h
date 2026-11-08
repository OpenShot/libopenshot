/**
 * @file
 * @brief Header file for AudioWaveformer class
 * @author Jonathan Thomas <jonathan@openshot.org>
 *
 * @ref License
 */

// Copyright (c) 2008-2022 OpenShot Studios, LLC
//
// SPDX-License-Identifier: LGPL-3.0-or-later

#ifndef OPENSHOT_WAVEFORMER_H
#define OPENSHOT_WAVEFORMER_H

#include "ReaderBase.h"
#include "Frame.h"
#include "KeyFrame.h"
#include "Fraction.h"
#include <memory>
#include <vector>
#include <string>


namespace openshot {

    /**
     * @brief This struct holds the extracted waveform data (both the RMS root-mean-squared average, and the max values)
     *
     * Because we extract 2 different datasets from the audio, we return this struct with access to both sets of data,
     * the average root mean squared values, and the max sample values.
     */
    struct AudioWaveformData
    {
        std::vector<float> max_samples;
        std::vector<float> rms_samples;

        /// Optional per-channel envelopes, filled by ExtractChannelSamples()
        std::vector<std::vector<float>> channel_max_samples;
        std::vector<std::vector<float>> channel_rms_samples;

        /// Resize both datasets
        void resize(int total_samples) {
            max_samples.resize(total_samples);
            rms_samples.resize(total_samples);
        }

        /// Zero out # of values in both datasets
        void zero(int total_samples) {
            std::fill(max_samples.begin(), max_samples.end(), 0);
            std::fill(rms_samples.begin(), rms_samples.end(), 0);
        }

        /// Scale # of values by some factor (including any per-channel envelopes)
        void scale(int total_samples, float factor) {
            for (auto s = 0; s < total_samples; s++) {
                max_samples[s] *= factor;
                rms_samples[s] *= factor;
            }
            for (auto& channel : channel_max_samples) {
                for (auto& value : channel) value *= factor;
            }
            for (auto& channel : channel_rms_samples) {
                for (auto& value : channel) value *= factor;
            }
        }

        /// Clear and free memory of all datasets
        void clear() {
            max_samples.clear();
            max_samples.shrink_to_fit();
            rms_samples.clear();
            rms_samples.shrink_to_fit();
            channel_max_samples.clear();
            channel_max_samples.shrink_to_fit();
            channel_rms_samples.clear();
            channel_rms_samples.shrink_to_fit();
        }

        /// Return a vector of vectors (containing both datasets)
        std::vector<std::vector<float>> vectors() {
            std::vector<std::vector<float>> output;
            output.push_back(max_samples);
            output.push_back(rms_samples);
            return output;
        }

        /// Number of per-channel envelopes available
        int channel_count() const {
            return static_cast<int>(channel_max_samples.size());
        }

        /// Return the max and RMS envelopes of one channel (empty when unavailable)
        std::vector<std::vector<float>> channel_vectors(int channel) const {
            std::vector<std::vector<float>> output(2);
            if (channel >= 0 && channel < channel_count()
                && channel < static_cast<int>(channel_rms_samples.size())) {
                output[0] = channel_max_samples[channel];
                output[1] = channel_rms_samples[channel];
            }
            return output;
        }
    };

    /**
     * @brief This class is used to extra audio data used for generating waveforms.
     *
     * Pass in a ReaderBase* with audio data, and this class will iterate the reader,
     * and sample down the dataset to a much smaller set - more useful for generating
     * waveforms. For example, take 44100 samples per second, and reduce it to 20
     * "max" or "average" samples per second - much easier to graph.
     */
    class AudioWaveformer {
    private:
        ReaderBase* reader;
        std::unique_ptr<ReaderBase> detached_reader; ///< Optional detached reader clone for waveform extraction
        ReaderBase* resolved_reader = nullptr;       ///< Cached pointer to the reader used for extraction
        bool source_initialized = false;

    public:
        /// Default constructor
        AudioWaveformer(ReaderBase* reader);

        /// @brief Extract audio samples from any ReaderBase class (legacy overload, now delegates to audio-only path)
        /// @param channel Which audio channel should we extract data from (-1 == all channels)
        /// @param num_per_second How many samples per second to return
        /// @param normalize Should we scale the data range so the largest value is 1.0
        AudioWaveformData ExtractSamples(int channel, int num_per_second, bool normalize);

        /// @brief Extract audio samples from a media file path (audio-only fast path)
        AudioWaveformData ExtractSamples(const std::string& path, int channel, int num_per_second, bool normalize);

        /// @brief Extract the mixed envelope and every channel's envelope in a single decode pass
        ///
        /// max_samples / rms_samples hold the all-channel envelope (the same values as
        /// ExtractSamples(-1, ...)), and channel_max_samples / channel_rms_samples hold one
        /// envelope per source channel (the same values as ExtractSamples(channel, ...)).
        /// Clip keyframes are NOT applied, since per-channel envelopes describe the source media.
        /// @param num_per_second How many samples per second to return
        /// @param normalize Scale all envelopes by the largest mixed peak, so it becomes 1.0
        AudioWaveformData ExtractChannelSamples(int num_per_second, bool normalize);

        /// @brief Apply time and volume keyframes to an existing waveform data set
        AudioWaveformData ApplyKeyframes(const AudioWaveformData& base,
                                         const openshot::Keyframe* time_keyframe,
                                         const openshot::Keyframe* volume_keyframe,
                                         const openshot::Fraction& project_fps,
                                         const openshot::Fraction& source_fps,
                                         int source_channels,
                                         int num_per_second,
                                         int channel,
                                         bool normalize);

        /// @brief Apply time and volume keyframes to existing max / RMS envelopes
        ///
        /// Keyframes are evaluated in the project-frame domain (1-based frames). Pass nullptr for
        /// either keyframe to skip it. Time curves with fewer than 2 points are ignored.
        AudioWaveformData ApplyKeyframes(const std::vector<float>& max_samples,
                                         const std::vector<float>& rms_samples,
                                         const openshot::Keyframe* time_keyframe,
                                         const openshot::Keyframe* volume_keyframe,
                                         const openshot::Fraction& project_fps,
                                         int num_per_second);

        /// @brief Convenience: extract then apply keyframes in one step from a file path
        AudioWaveformData ExtractSamples(const std::string& path,
                                         const openshot::Keyframe* time_keyframe,
                                         const openshot::Keyframe* volume_keyframe,
                                         const openshot::Fraction& project_fps,
                                         int channel,
                                         int num_per_second,
                                         bool normalize);

        /// Destructor
        ~AudioWaveformer();

    private:
        AudioWaveformData ExtractSamplesFromReader(openshot::ReaderBase* source_reader, int channel, int num_per_second, bool normalize, bool per_channel = false);
        openshot::ReaderBase* ResolveSourceReader(openshot::ReaderBase* source_reader);
        openshot::Fraction ResolveSourceFPS(openshot::ReaderBase* source_reader);
        openshot::ReaderBase* ResolveWaveformReader();
    };

}

#endif
