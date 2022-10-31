/**
 * @file
 * @brief Unit tests for openshot::AudioWaveformer
 * @author Jonathan Thomas <jonathan@openshot.org>
 *
 * @ref License
 */

// Copyright (c) 2008-2019 OpenShot Studios, LLC
//
// SPDX-License-Identifier: LGPL-3.0-or-later

#include "openshot_catch.h"
#include "AudioWaveformer.h"
#include "FFmpegReader.h"


using namespace openshot;

TEST_CASE( "Extract waveform data piano.wav", "[libopenshot][audiowaveformer]" )
{
    // Create a reader
    std::stringstream path;
    path << TEST_MEDIA_PATH << "piano.wav";
    FFmpegReader r(path.str());
    r.Open();

    // Create AudioWaveformer and extract a smaller "average" sample set of audio data
    AudioWaveformer waveformer(&r);
    for (auto channel = 0; channel < r.info.channels; channel++) {
        std::vector<float> waveform = waveformer.ExtractSamples(channel, 20, false);

        if (channel == 0) {
            CHECK(waveform.size() == 107);
            CHECK(waveform[0] == Approx(0.000820312474f).margin(0.00001));
            CHECK(waveform[86] == Approx(-0.00144531252f).margin(0.00001));
            CHECK(waveform[87] == Approx(0.0f).margin(0.00001));

            for (auto sample = 0; sample < waveform.size(); sample++) {
                std::cout << waveform[sample] << std::endl;
            }
        } else if (channel == 1) {
            CHECK(waveform.size() == 107);
            CHECK(waveform[0] == Approx(0.000820312474f).margin(0.00001));
            CHECK(waveform[86] == Approx(-0.00144531252f).margin(0.00001));
            CHECK(waveform[87] == Approx(0.0f).margin(0.00001));
        }

        waveform.clear();
    }

    // Clean up
    r.Close();
}

TEST_CASE( "Extract waveform data sintel", "[libopenshot][audiowaveformer]" )
{
    // Create a reader
    std::stringstream path;
    path << TEST_MEDIA_PATH << "sintel_trailer-720p.mp4";
    FFmpegReader r(path.str());

    // Create AudioWaveformer and extract a smaller "average" sample set of audio data
    AudioWaveformer waveformer(&r);
    for (auto channel = 0; channel < r.info.channels; channel++) {
        std::vector<float> waveform = waveformer.ExtractSamples(channel, 20, false);

        if (channel == 0) {
            CHECK(waveform.size() == 1058);
            CHECK(waveform[0] == Approx(-1.48391728e-05f).margin(0.00001));
            CHECK(waveform[1037] == Approx(6.79016102e-06f).margin(0.00001));
            CHECK(waveform[1038] == Approx(0.0f).margin(0.00001));
        } else if (channel == 1) {
            CHECK(waveform.size() == 1058);
            CHECK(waveform[0] == Approx(-1.43432617e-05f).margin(0.00001));
            CHECK(waveform[1037] == Approx(6.79016102e-06f).margin(0.00001));
            CHECK(waveform[1038] == Approx(0.0f).margin(0.00001));
        }

        waveform.clear();
    }

    // Clean up
    r.Close();
}

TEST_CASE( "Normalize & scale waveform data piano.wav", "[libopenshot][audiowaveformer]" )
{
    // Create a reader
    std::stringstream path;
    path << TEST_MEDIA_PATH << "piano.wav";
    FFmpegReader r(path.str());

    // Create AudioWaveformer and extract a smaller "average" sample set of audio data
    AudioWaveformer waveformer(&r);
    for (auto channel = 0; channel < r.info.channels; channel++) {
        // Normalize values and scale them between -1 and +1
        std::vector<float> waveform = waveformer.ExtractSamples(channel, 20, true);

        if (channel == 0) {
            CHECK(waveform.size() == 107);
            CHECK(waveform[0] == Approx(0.113821134).margin(0.00001));
            CHECK(waveform[35] == Approx(-1.0f).margin(0.00001));
            CHECK(waveform[86] == Approx(-0.200542003f).margin(0.00001));
            CHECK(waveform[87] == Approx(0.0f).margin(0.00001));
        }

        waveform.clear();
    }

    // Clean up
    r.Close();
}
