/**
 * @file
 * @brief Duration-preserving pitch shifting with musical automation.
 */
// Copyright (c) 2026 OpenShot Studios, LLC
// SPDX-License-Identifier: LGPL-3.0-or-later

#ifndef OPENSHOT_PITCH_SHIFT_H
#define OPENSHOT_PITCH_SHIFT_H

#include "EffectBase.h"
#include "KeyFrame.h"
#include <memory>

namespace openshot {

/// Raise or lower pitch without changing speed or duration.
class PitchShift : public EffectBase {
public:
    enum Quality { FAST = 0, BALANCED = 1, HIGH = 2 };

    Keyframe pitch; ///< Semitones, -24 to +24. Fractional values allow fine tuning.
    Keyframe mix;   ///< Linear dry/wet blend, 0 to 1.
    bool preserve_formants = true;
    int quality = BALANCED;

    PitchShift();
    ~PitchShift() override;

    /// Clip-hosted use supplies neighboring audio for continuous, aligned output.
    /// An unparented frame is treated as an isolated buffer with silence outside.
    std::shared_ptr<Frame> GetFrame(int64_t frame_number) override;
    std::shared_ptr<Frame> GetFrame(std::shared_ptr<Frame> frame, int64_t frame_number) override;
    std::string Json() const override;
    Json::Value JsonValue() const override;
    void SetJson(const std::string value) override;
    void SetJsonValue(const Json::Value root) override;
    std::string PropertiesJSON(int64_t requested_frame) const override;

private:
    struct State;
    std::unique_ptr<State> state;
};

} // namespace openshot
#endif
