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


/**
 * @brief This struct hold information about Audio Devices
 *
 * The type and name of the audio device.
 */
namespace openshot {
	struct AudioDeviceInfo
	{
		std::string name;
		std::string type;
	};
}
#endif
