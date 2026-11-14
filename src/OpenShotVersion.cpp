/**
 * @file
 * @brief Source file for GetVersion function
 * @author Jonathan Thomas <jonathan@openshot.org>
 * @author FeRD (Frank Dana) <ferdnyc@gmail.com>
 *
 * @ref License
 */

// Copyright (c) 2008-2019 OpenShot Studios, LLC
//
// SPDX-License-Identifier: LGPL-3.0-or-later

#include "OpenShotVersion.h"
#include <OpenShotAudioVersion.h>

namespace openshot {
    std::string GetAudioVersion() {
        return openshot_audio::GetVersion();
    }

    OpenShotVersion GetVersion() {
        return openshot::Version;
    }
}