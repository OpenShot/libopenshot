// SPDX-License-Identifier: LGPL-3.0-or-later
// Internal audio initialization shared with deterministic device-manager tests.
#ifndef OPENSHOT_AUDIO_DEVICE_INITIALIZATION_H
#define OPENSHOT_AUDIO_DEVICE_INITIALIZATION_H
#include "AudioDevices.h"
#include <juce_audio_devices/juce_audio_devices.h>
namespace openshot { namespace detail {
struct AudioDeviceResult {
    AudioDeviceInfo device;
    double sample_rate = 0.0;
    std::string error;
};
AudioDeviceResult InitializeAudioDevice(juce::AudioDeviceManager& manager,
    AudioDeviceInfo requested_device, int rate, int channels, int buffer_size);
}}
#endif
