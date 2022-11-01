/**
 * @file
 * @brief Unit tests for openshot::AudioWaveformer
 * @author Jonathan Thomas <jonathan@openshot.org>
 *
 * @ref License
 */

// Copyright (c) 2008-2022 OpenShot Studios, LLC
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
        AudioWaveformData waveform = waveformer.ExtractSamples(channel, 20, false);

        if (channel == 0) {
            CHECK(waveform.rms_samples.size() == 107);
            CHECK(waveform.rms_samples[0] == Approx(0.04879f).margin(0.00001));
            CHECK(waveform.rms_samples[86] == Approx(0.13578f).margin(0.00001));
            CHECK(waveform.rms_samples[87] == Approx(0.0f).margin(0.00001));
        } else if (channel == 1) {
            CHECK(waveform.rms_samples.size() == 107);
            CHECK(waveform.rms_samples[0] == Approx(0.04879f).margin(0.00001));
            CHECK(waveform.rms_samples[86] == Approx(0.13578f).margin(0.00001));
            CHECK(waveform.rms_samples[87] == Approx(0.0f).margin(0.00001));
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
        AudioWaveformData waveform = waveformer.ExtractSamples(channel, 20, false);

        if (channel == 0) {
            CHECK(waveform.rms_samples.size() == 1058);
            CHECK(waveform.rms_samples[0] == Approx(0.00001f).margin(0.00001));
            CHECK(waveform.rms_samples[1037] == Approx(0.00003f).margin(0.00001));
            CHECK(waveform.rms_samples[1038] == Approx(0.0f).margin(0.00001));
        } else if (channel == 1) {
            CHECK(waveform.rms_samples.size() == 1058);
            CHECK(waveform.rms_samples[0] == Approx(0.00001f ).margin(0.00001));
            CHECK(waveform.rms_samples[1037] == Approx(0.00003f).margin(0.00001));
            CHECK(waveform.rms_samples[1038] == Approx(0.0f).margin(0.00001));
        }

        waveform.clear();
    }

    // Clean up
    r.Close();
}


TEST_CASE( "Extract waveform data sintel (all channels)", "[libopenshot][audiowaveformer]" )
{
    // Create a reader
    std::stringstream path;
    path << TEST_MEDIA_PATH << "sintel_trailer-720p.mp4";
    FFmpegReader r(path.str());

    // Create AudioWaveformer and extract a smaller "average" sample set of audio data
    AudioWaveformer waveformer(&r);
    AudioWaveformData waveform = waveformer.ExtractSamples(-1, 20, false);

    CHECK(waveform.rms_samples.size() == 1058);
    CHECK(waveform.rms_samples[0] == Approx(0.00001f).margin(0.00001));
    CHECK(waveform.rms_samples[1037] == Approx(0.00003f).margin(0.00001));
    CHECK(waveform.rms_samples[1038] == Approx(0.0f).margin(0.00001));

    waveform.clear();

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
        AudioWaveformData waveform = waveformer.ExtractSamples(channel, 20, true);

        if (channel == 0) {
            CHECK(waveform.rms_samples.size() == 107);
            CHECK(waveform.rms_samples[0] == Approx(0.07524f).margin(0.00001));
            CHECK(waveform.rms_samples[35] == Approx(0.20063f).margin(0.00001));
            CHECK(waveform.rms_samples[86] == Approx(0.2094f).margin(0.00001));
            CHECK(waveform.rms_samples[87] == Approx(0.0f).margin(0.00001));
        }

        waveform.clear();
    }

    // Clean up
    r.Close();
}
