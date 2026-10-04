// SPDX-License-Identifier: LGPL-3.0-or-later
#ifndef OPENSHOT_ADAPTIVE_PREROLL_H
#define OPENSHOT_ADAPTIVE_PREROLL_H

#include <algorithm>
#include <cmath>
#include <cstdint>

namespace openshot {
// No clocks, readers, or worker ownership: callers supply monotonic microseconds.
// All access is serialized by VideoCacheThread's existing seek-state mutex.
class AdaptivePreroll {
public:
    static constexpr int64_t StartupLimitUs = 500000;
    static constexpr int64_t MaxProbeFrames = 120;
    void Reset(int64_t now) {
        started = now;
        samples = 0;
        mean = deviation = 0;
        target = 0;
        released = false;
        timed_out = false;
    }
    void Observe(int64_t duration_us) {
        if (duration_us < 0) return;
        const double value = static_cast<double>(duration_us);
        if (!samples) mean = value;
        else {
            deviation += 0.25 * (std::abs(value - mean) - deviation);
            mean += 0.25 * (value - mean);
        }
        ++samples;
    }
    // Capacity is the worker's reachable directional window, excluding playhead.
    int64_t Target(double fps, int speed, int64_t capacity,
                   int64_t conservative, int64_t maximum) {
        const int64_t stride = std::max<int64_t>(1, std::abs(static_cast<int64_t>(speed)));
        const int64_t limit = std::max<int64_t>(0, std::min({capacity, maximum, MaxProbeFrames}));
        if (conservative < 0 || limit == 0) return target = 0;
        int64_t wanted = std::min(conservative, limit);
        if (samples >= 3 && fps > 0) {
            const double budget = 1000000.0 / fps / stride;
            const double cost = (mean + 3 * deviation) / budget;
            const double demand = cost < 0.5 ? 2.0 * stride
                : std::ceil(2 + 4 * cost) * stride;
            // Clamp in floating point before conversion, including corrupt or
            // extreme timing/speed metadata, so demand cannot overflow int64.
            wanted = static_cast<int64_t>(std::min<double>(limit, demand));
        }
        wanted = std::clamp<int64_t>(wanted, 0, limit);
        // Demand rises promptly; decay only after a two-frame difference.
        if (!target || wanted > target || wanted + 2 <= target) target = wanted;
        return target = std::min(target, limit);
    }
    bool Ready(int64_t now, int64_t contiguous_ahead, bool current_available,
               int64_t required, int64_t minimum_step = 1) {
        timed_out = now - started >= StartupLimitUs;
        const int64_t threshold = released || timed_out ? minimum_step : required;
        const bool ready = current_available && contiguous_ahead >= threshold;
        if (ready) released = true;
        return ready;
    }
    bool TimedOut() const { return timed_out; }
    bool Released() const { return released; }
private:
    int64_t started = 0, target = 0;
    unsigned samples = 0;
    double mean = 0, deviation = 0;
    bool released = false, timed_out = false;
};
}
#endif
