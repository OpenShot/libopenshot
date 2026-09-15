/**
 * @file
 * @brief Unit tests for openshot::AudioDeviceManagerSingleton
 * @author Jonathan Thomas <jonathan@openshot.org>
 *
 * @ref License
 */

// Copyright (c) 2008-2022 OpenShot Studios, LLC
//
// SPDX-License-Identifier: LGPL-3.0-or-later

#include "openshot_catch.h"
#include "Settings.h"
#include "Qt/AudioPlaybackThread.h"


using namespace openshot;

TEST_CASE( "Initialize Audio Device Manager Singleton", "[libopenshot][AudioDeviceManagerSingleton]" )
{
    const auto require_supported_rate = [](AudioDeviceManagerSingleton* manager, double requested_rate) {
        auto* device = manager->audioDeviceManager.getCurrentAudioDevice();
        CHECK(device != nullptr);
        if (!device) {
            return;
        }

        const double actual_rate = device->getCurrentSampleRate();
        INFO("requested_rate=" << requested_rate);
        INFO("actual_rate=" << actual_rate);
        INFO("device_name=" << device->getName());
        INFO("device_type=" << device->getTypeName());

        CHECK(manager->defaultSampleRate == actual_rate);
        const bool rate_is_supported =
            actual_rate == Approx(requested_rate).margin(0.5) ||
            actual_rate == Approx(48000.0).margin(0.5) ||
            actual_rate == Approx(44100.0).margin(0.5) ||
            actual_rate == Approx(22050.0).margin(0.5);
        CHECK(rate_is_supported);
    };

    Settings::Instance()->PLAYBACK_AUDIO_DEVICE_TYPE = "";
    Settings::Instance()->PLAYBACK_AUDIO_DEVICE_NAME = "";

    // Invalid sample rate
    AudioDeviceManagerSingleton *mng = AudioDeviceManagerSingleton::Instance(12300, 2);
    double detected_sample_rate = mng->defaultSampleRate;

    // Ignore systems that fail to find a valid audio device (i.e. build servers w/no sound card)
    if (mng->initialise_error.empty() && detected_sample_rate >= 44100.0) {
        // Verify invalid sample rate not found
        CHECK(detected_sample_rate != 12300); // verify common rate is returned
        mng->CloseAudioDevice();

        // Valid sample rate
        mng = AudioDeviceManagerSingleton::Instance(44100, 2);
        require_supported_rate(mng, 44100.0);
        mng->CloseAudioDevice();

        // Valid device type (for Linux)
        Settings::Instance()->PLAYBACK_AUDIO_DEVICE_TYPE = "ALSA";
        Settings::Instance()->PLAYBACK_AUDIO_DEVICE_NAME = "Playback/recording through the PulseAudio sound server";
        mng = AudioDeviceManagerSingleton::Instance(44100, 2);
        if (mng->currentAudioDevice.get_name() == Settings::Instance()->PLAYBACK_AUDIO_DEVICE_NAME &&
            mng->currentAudioDevice.get_type() == Settings::Instance()->PLAYBACK_AUDIO_DEVICE_TYPE) {
            // Only check this device if it exists (i.e. we are on Linux with ALSA and PulseAudio)
            require_supported_rate(mng, 44100.0);
        }
        mng->CloseAudioDevice();

        // Invalid device type (for Linux)
        Settings::Instance()->PLAYBACK_AUDIO_DEVICE_TYPE = "Fake Type";
        Settings::Instance()->PLAYBACK_AUDIO_DEVICE_NAME = "Fake Device";
        mng = AudioDeviceManagerSingleton::Instance(44100, 2);
        require_supported_rate(mng, 44100.0);
        mng->CloseAudioDevice();
    }
}
