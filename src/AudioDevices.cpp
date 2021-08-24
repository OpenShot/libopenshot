/**
 * @file
 * @brief Utility methods for identifying audio devices
 * @author Jonathan Thomas <jonathan@openshot.org>
 * @author FeRD (Frank Dana) <ferdnyc@gmail.com>
 *
 * @ref License
 */

/* LICENSE
 *
 * Copyright (c) 2008-2021 OpenShot Studios, LLC
 * <http://www.openshotstudios.com/>. This file is part of
 * OpenShot Library (libopenshot), an open-source project dedicated to
 * delivering high quality video editing and animation solutions to the
 * world. For more information visit <http://www.openshot.org/>.
 *
 * OpenShot Library (libopenshot) is free software: you can redistribute it
 * and/or modify it under the terms of the GNU Lesser General Public License
 * as published by the Free Software Foundation, either version 3 of the
 * License, or (at your option) any later version.
 *
 * OpenShot Library (libopenshot) is distributed in the hope that it will be
 * useful, but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public License
 * along with OpenShot Library. If not, see <http://www.gnu.org/licenses/>.
 */

#include "AudioDevices.h"

#include <OpenShotAudio.h>

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
