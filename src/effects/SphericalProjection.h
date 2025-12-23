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
#include <vector>

namespace openshot {

/**
 * @brief Projects 360° or fisheye video through a virtual camera.
 * Supports yaw, pitch, roll, input and output FOV, sphere/hemisphere/fisheye
 * modes, optional inversion, and automatic quality selection.
 */
class SphericalProjection : public EffectBase {
private:
  void init_effect_details();

public:
  // Enums
  enum InputModel {
    INPUT_EQUIRECT          = 0,
    INPUT_FEQ_EQUIDISTANT   = 1, // r = f * theta
    INPUT_FEQ_EQUISOLID     = 2, // r = 2f * sin(theta/2)
    INPUT_FEQ_STEREOGRAPHIC = 3, // r = 2f * tan(theta/2)
    INPUT_FEQ_ORTHOGRAPHIC  = 4  // r = f * sin(theta)
  };

  enum ProjectionMode {
    MODE_RECT_SPHERE            = 0, // Rectilinear view over full sphere
    MODE_RECT_HEMISPHERE        = 1, // Rectilinear view over hemisphere
    MODE_FISHEYE_EQUIDISTANT    = 2, // Output fisheye (equidistant)
    MODE_FISHEYE_EQUISOLID      = 3, // Output fisheye (equisolid)
    MODE_FISHEYE_STEREOGRAPHIC  = 4, // Output fisheye (stereographic)
    MODE_FISHEYE_ORTHOGRAPHIC   = 5  // Output fisheye (orthographic)
  };

  enum InterpMode {
    INTERP_NEAREST  = 0,
    INTERP_BILINEAR = 1,
    INTERP_BICUBIC  = 2,
    INTERP_AUTO     = 3
  };

  enum InvertFlag {
    INVERT_NORMAL = 0,
    INVERT_BACK   = 1
  };

  Keyframe yaw;    ///< Yaw around up-axis (degrees)
  Keyframe pitch;  ///< Pitch around right-axis (degrees)
  Keyframe roll;   ///< Roll around forward-axis (degrees)
  Keyframe fov;    ///<  Output field-of-view (degrees)
  Keyframe in_fov; ///< Source lens coverage / FOV (degrees)

  int projection_mode; ///< 0=Sphere, 1=Hemisphere, 2=Fisheye
  int invert;          ///< 0=Normal, 1=Invert (back lens / +180°)
  int input_model;     ///< 0=Equirect, 1=Fisheye-Equidistant
  int interpolation;   ///< 0=Nearest, 1=Bilinear, 2=Bicubic, 3=Auto

  /// Blank ctor (for JSON deserialization)
  SphericalProjection();

  /// Ctor with custom curves
  SphericalProjection(Keyframe new_yaw, Keyframe new_pitch, Keyframe new_roll,
                      Keyframe new_fov);

  /// ClipBase override: create a fresh Frame then call the main GetFrame
  std::shared_ptr<Frame> GetFrame(int64_t frame_number) override {
    return GetFrame(std::make_shared<Frame>(), frame_number);
  }

  /// EffectBase override: reproject the QImage
  std::shared_ptr<Frame> GetFrame(std::shared_ptr<Frame> frame,
                                  int64_t frame_number) override;

  // JSON serialization
  std::string Json() const override;
  void SetJson(std::string value) override;
  Json::Value JsonValue() const override;
  void SetJsonValue(Json::Value root) override;
  std::string PropertiesJSON(int64_t requested_frame) const override;

private:
  void project_input(double dx, double dy, double dz, double in_fov_r, int W,
                     int H, double &uf, double &vf) const;

  mutable std::vector<float> uv_map; ///< Cached UV lookup
  mutable int cached_width = 0;
  mutable int cached_height = 0;
  mutable double cached_yaw = 0.0, cached_pitch = 0.0, cached_roll = 0.0;
  mutable double cached_in_fov = 0.0, cached_out_fov = 0.0;
  mutable int cached_input_model = -1;
  mutable int cached_projection_mode = -1;
  mutable int cached_invert = -1;
};

} // namespace openshot

#endif // OPENSHOT_SPHERICAL_PROJECTION_EFFECT_H
