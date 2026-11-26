// Copyright (c) 2008-2026 OpenShot Studios, LLC
// SPDX-License-Identifier: LGPL-3.0-or-later

#include "AudioReactiveKeyframes.h"

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <stdexcept>

namespace openshot {

Keyframe AudioReactiveKeyframes::Generate(
    const std::vector<float>& energy,
    const Keyframe& base,
    const AudioReactiveOptions& options)
{
    if (!std::isfinite(options.gain) ||
        !std::isfinite(options.threshold) ||
        !std::isfinite(options.amount) ||
        options.gain < 0.0 ||
        options.threshold < 0.0 ||
        options.threshold >= 1.0 ||
        options.smoothing_frames < 1 ||
        (options.mode != AudioReactiveMode::SCALE &&
         options.mode != AudioReactiveMode::LOCATION)) {
        throw std::invalid_argument(
            "Invalid audio-reactive options");
    }

    if (energy.empty())
        return base;

    Keyframe result;

    const size_t window =
        static_cast<size_t>(options.smoothing_frames);

    double running_sum = 0.0;

    for (size_t i = 0; i < energy.size(); ++i) {
        const double incoming = std::isfinite(energy[i])
            ? std::clamp(
                static_cast<double>(energy[i]), 0.0, 1.0)
            : 0.0;

        running_sum += incoming;

        if (i >= window) {
            const double outgoing =
                std::isfinite(energy[i - window])
                    ? std::clamp(
                        static_cast<double>(energy[i - window]),
                        0.0, 1.0)
                    : 0.0;

            running_sum -= outgoing;
        }

        const size_t count = std::min(i + 1, window);
        const double smoothed = running_sum / count;

        const double amplified =
            std::clamp(smoothed * options.gain, 0.0, 1.0);

        const double response =
            amplified <= options.threshold
                ? 0.0
                : (amplified - options.threshold) /
                  (1.0 - options.threshold);

        const int64_t frame =
            static_cast<int64_t>(i + 1);

        const double base_value =
            base.GetValue(frame);

        double value = base_value;

        switch (options.mode) {
        case AudioReactiveMode::SCALE:
            value = base_value *
                (1.0 + options.amount * response);
            break;

        case AudioReactiveMode::LOCATION:
            value = base_value +
                options.amount * response;
            break;
        }

        if (!std::isfinite(value)) {
            throw std::overflow_error(
                "Audio-reactive keyframe value is not finite");
        }

        result.AddPoint(frame, value, LINEAR);
    }

    return result;
}

} // namespace openshot
