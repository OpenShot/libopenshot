/**
 * @file
 * @brief Header file for SphericalProjection effect class
 * @author Jonathan Thomas <jonathan@openshot.org>
 *
 * @ref License
 */

// Copyright (c) 2008-2025 OpenShot Studios, LLC
//
// SPDX-License-Identifier: LGPL-3.0-or-later

#ifndef OPENSHOT_SPHERICAL_PROJECTION_EFFECT_H
#define OPENSHOT_SPHERICAL_PROJECTION_EFFECT_H

#include "../EffectBase.h"
#include "../Frame.h"
#include "../Json.h"
#include "../KeyFrame.h"

#include <memory>
#include <string>

namespace openshot
{

/**
 * @brief Projects 360° or fisheye video through a virtual camera.
 * Supports yaw, pitch, roll, FOV, sphere/hemisphere/fisheye modes,
 * optional inversion, and nearest/bilinear sampling.
 */
class SphericalProjection : public EffectBase
{
private:
    void init_effect_details();

public:
    Keyframe yaw;           ///< Yaw around up-axis (degrees)
    Keyframe pitch;         ///< Pitch around right-axis (degrees)
    Keyframe roll;          ///< Roll around forward-axis (degrees)
    Keyframe fov;           ///< Field-of-view (horizontal, degrees)

    int projection_mode;    ///< 0=Sphere, 1=Hemisphere, 2=Fisheye
    int invert;             ///< 0=Normal, 1=Invert (back lens / +180°)
    int interpolation;      ///< 0=Nearest, 1=Bilinear

    /// Blank ctor (for JSON deserialization)
    SphericalProjection();

    /// Ctor with custom curves
    SphericalProjection(Keyframe new_yaw,
                        Keyframe new_pitch,
                        Keyframe new_roll,
                        Keyframe new_fov);

    /// ClipBase override: create a fresh Frame then call the main GetFrame
    std::shared_ptr<Frame> GetFrame(int64_t frame_number) override
    { return GetFrame(std::make_shared<Frame>(), frame_number); }

    /// EffectBase override: reproject the QImage
    std::shared_ptr<Frame> GetFrame(std::shared_ptr<Frame> frame,
                                    int64_t frame_number) override;

    // JSON serialization
    std::string Json() const override;
    void SetJson(std::string value) override;
    Json::Value JsonValue() const override;
    void SetJsonValue(Json::Value root) override;
    std::string PropertiesJSON(int64_t requested_frame) const override;
};

} // namespace openshot

#endif // OPENSHOT_SPHERICAL_PROJECTION_EFFECT_H
