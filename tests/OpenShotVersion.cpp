/**
 * @file
 * @brief Verify that libopenshot reports the loaded audio library version.
 * @author OpenShot Studios, LLC
 */

// SPDX-FileCopyrightText: 2026 OpenShot Studios, LLC
// SPDX-License-Identifier: LGPL-3.0-or-later

#include "openshot_catch.h"
#include "OpenShotVersion.h"
#include <OpenShotAudioVersion.h>
#include <string>

TEST_CASE("Audio version identifies the loaded library", "[version]") {
    const std::string audio_version = openshot_audio::GetVersion();
    REQUIRE_FALSE(audio_version.empty());
    CHECK(openshot::GetAudioVersion() == audio_version);
}
