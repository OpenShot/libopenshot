/**
 * @file
 * @brief Utility methods for identifying audio devices
 * @author Jonathan Thomas <jonathan@openshot.org>
 * @author FeRD (Frank Dana) <ferdnyc@gmail.com>
 *
 * @ref License
 */

// Copyright (c) 2008-2019 OpenShot Studios, LLC
//
// SPDX-License-Identifier: LGPL-3.0-or-later

#include "AudioDevices.h"

using namespace openshot;

using AudioDeviceList = std::vector<std::pair<std::string, std::string>>;

// Build a list of devices found, and return
AudioDeviceList AudioDevices::getNames() {
    // A temporary device manager, used to scan device names.
    // Its initialize() is never called, and devices are not opened.
    std::unique_ptr<juce::AudioDeviceManager>
        manager(new juce::AudioDeviceManager());

    m_devices.clear();

    auto &types = manager->getAvailableDeviceTypes();
    for (auto* t : types) {
        t->scanForDevices();
        const auto names = t->getDeviceNames();
        for (const auto& name : names) {
            m_devices.emplace_back(
                name.toStdString(), t->getTypeName().toStdString());
        }
    }
    return m_devices;
}
