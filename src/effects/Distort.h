// Copyright (c) 2026 OpenShot Studios, LLC
// SPDX-License-Identifier: LGPL-3.0-or-later
#ifndef OPENSHOT_DISTORT_H
#define OPENSHOT_DISTORT_H
#include "../EffectBase.h"
#include "../Frame.h"
#include "../KeyFrame.h"
#include <array>

namespace openshot {
/// Smooth 4x4 mesh warp. Offsets are fractions of the source dimensions.
/// Presets and custom offsets are additive, then multiplied by strength.
class Distort : public EffectBase {
    void init_effect_details();
public:
    int preset = 0; ///< Custom, Bend, Flag, Bulge, Pinch, Taper, Squeeze
    int selected_point = 5; ///< Inspector selection only; does not affect pixels
    Keyframe strength;
    std::array<Keyframe, 16> points_x;
    std::array<Keyframe, 16> points_y;
    Distort();
    bool RendersOnVulkan() const override { return true; }
    std::shared_ptr<Frame> GetFrame(int64_t number) override {
        return GetFrame(std::make_shared<Frame>(), number);
    }
    std::shared_ptr<Frame> GetFrame(std::shared_ptr<Frame> frame, int64_t number) override;
    std::string Json() const override;
    Json::Value JsonValue() const override;
    void SetJson(const std::string value) override;
    void SetJsonValue(const Json::Value root) override;
    std::string PropertiesJSON(int64_t number) const override;
    /// Evaluated surface, shared by CPU rendering and the preview overlay.
    /// Returns a row-major (divisions+1)^2 array of normalized [x,y] pairs.
    std::string MeshJSON(int64_t number, int divisions = 36) const;
};
}
#endif
