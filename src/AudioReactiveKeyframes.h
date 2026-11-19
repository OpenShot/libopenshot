// Copyright (c) 2008-2026 OpenShot Studios, LLC
// SPDX-License-Identifier: LGPL-3.0-or-later

#ifndef OPENSHOT_AUDIO_REACTIVE_KEYFRAMES_H
#define OPENSHOT_AUDIO_REACTIVE_KEYFRAMES_H

#include "KeyFrame.h"

#include <vector>

namespace openshot {

enum class AudioReactiveMode {
    SCALE,
    LOCATION
};

struct AudioReactiveOptions {
    double gain = 1.0;
    double threshold = 0.0;
    double amount = 0.25;
    int smoothing_frames = 1;
    AudioReactiveMode mode = AudioReactiveMode::SCALE;
};

class AudioReactiveKeyframes {
public:
    // Each energy sample corresponds to one output video frame.
    // The first generated point is at frame 1.
    //
    // SCALE:    output = base * (1 + amount * response)
    // LOCATION: output = base + amount * response
    //
    // The input base curve is never modified.
    static Keyframe Generate(
        const std::vector<float>& energy,
        const Keyframe& base,
        const AudioReactiveOptions& options);
};

} // namespace openshot

#endif
