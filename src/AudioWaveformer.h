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
#include <vector>


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

        /// Scale # of values by some factor
        void scale(int total_samples, float factor) {
            for (auto s = 0; s < total_samples; s++) {
                max_samples[s] *= factor;
                rms_samples[s] *= factor;
            }
        }

        /// Clear and free memory of both datasets
        void clear() {
            max_samples.clear();
            max_samples.shrink_to_fit();
            rms_samples.clear();
            rms_samples.shrink_to_fit();
        }

        /// Return a vector of vectors (containing both datasets)
        std::vector<std::vector<float>> vectors() {
            std::vector<std::vector<float>> output;
            output.push_back(max_samples);
            output.push_back(rms_samples);
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

    public:
        /// Default constructor
        AudioWaveformer(ReaderBase* reader);

        /// @brief Extract audio samples from any ReaderBase class
        /// @param channel Which audio channel should we extract data from (-1 == all channels)
        /// @param num_per_second How many samples per second to return
        /// @param normalize Should we scale the data range so the largest value is 1.0
        AudioWaveformData ExtractSamples(int channel, int num_per_second, bool normalize);

        /// Destructor
        ~AudioWaveformer();
    };

}

#endif
