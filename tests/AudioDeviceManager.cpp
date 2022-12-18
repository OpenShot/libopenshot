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
    Settings::Instance()->PLAYBACK_AUDIO_DEVICE_TYPE = "";
    Settings::Instance()->PLAYBACK_AUDIO_DEVICE_NAME = "";

    // Invalid sample rate
    AudioDeviceManagerSingleton *mng = AudioDeviceManagerSingleton::Instance(12300, 2);
    CHECK((mng->defaultSampleRate == 48000 || mng->defaultSampleRate == 44100)); // verify common rate is returned
    mng->CloseAudioDevice();

    // Valid sample rate
    mng = AudioDeviceManagerSingleton::Instance(44100, 2);
    CHECK(mng->defaultSampleRate == 44100);
    mng->CloseAudioDevice();

    // Valid device type (for Linux)
    Settings::Instance()->PLAYBACK_AUDIO_DEVICE_TYPE = "ALSA";
    Settings::Instance()->PLAYBACK_AUDIO_DEVICE_NAME = "Playback/recording through the PulseAudio sound server";
    mng = AudioDeviceManagerSingleton::Instance(44100, 2);
    if (mng->currentAudioDevice.get_name() == Settings::Instance()->PLAYBACK_AUDIO_DEVICE_NAME &&
        mng->currentAudioDevice.get_type() == Settings::Instance()->PLAYBACK_AUDIO_DEVICE_TYPE) {
        // Only check this device if it exists (i.e. we are on Linux with ALSA and PulseAudio)
        CHECK(mng->defaultSampleRate == 44100);
        mng->CloseAudioDevice();
    }

    // Invalid device type (for Linux)
    Settings::Instance()->PLAYBACK_AUDIO_DEVICE_TYPE = "Fake Type";
    Settings::Instance()->PLAYBACK_AUDIO_DEVICE_NAME = "Fake Device";
    mng = AudioDeviceManagerSingleton::Instance(44100, 2);
    CHECK(mng->defaultSampleRate == 44100);
    mng->CloseAudioDevice();
}
