/**
 * @file
 * @brief Header file for Audio Device Info struct
 * @author Jonathan Thomas <jonathan@openshot.org>
 *
 * @ref License
 */

// Copyright (c) 2008-2019 OpenShot Studios, LLC
//
// SPDX-License-Identifier: LGPL-3.0-or-later

#ifndef OPENSHOT_AUDIODEVICEINFO_H
#define OPENSHOT_AUDIODEVICEINFO_H

#include <string>
#include <vector>

namespace openshot {
/**
 * @brief This struct hold information about Audio Devices
 *
 * The type and name of the audio device.
 */
struct
AudioDeviceInfo {
	std::string name;
	std::string type;
};

using AudioDeviceList = std::vector<std::pair<std::string, std::string>>;

	/// A class which probes the available audio devices
class AudioDevices
{
public:
        AudioDevices() = default;

	/// Return a vector of std::pair<> objects holding the
	/// device name and type for each audio device detected
	AudioDeviceList getNames();
private:
	AudioDeviceList m_devices;
};

}
#endif
