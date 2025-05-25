// Sharpen.h
/**
 * @file
 * @brief Header file for Sharpen effect class
 * @author Jonathan Thomas <jonathan@openshot.org>
 *
 * @ref License
 */

// Copyright (c) 2008-2025 OpenShot Studios, LLC
//
// SPDX-License-Identifier: LGPL-3.0-or-later

#ifndef OPENSHOT_SHARPEN_EFFECT_H
#define OPENSHOT_SHARPEN_EFFECT_H

#include "EffectBase.h"
#include "KeyFrame.h"
#include "Json.h"

#include <string>

namespace openshot {

/**
 * @brief This class provides a sharpen effect for video frames.
 *
 * The sharpen effect enhances the edges and details in a video frame, making it appear sharper.
 * It uses an unsharp mask or high-pass blend technique with adjustable parameters.
 */
class Sharpen : public EffectBase {
private:
    /// Initialize the effect details
    void init_effect_details();

public:
    /// Amount of sharpening to apply (0 to 2)
    Keyframe amount;

    /// Radius of the blur used in sharpening (0 to 10 pixels for 1080p)
    Keyframe radius;

    /// Threshold for applying sharpening (0 to 1)
    Keyframe threshold;

    /// Sharpening mode (0 = UnsharpMask, 1 = HighPassBlend)
    int mode;

    /// Channel to apply sharpening to (0 = All, 1 = Luma, 2 = Chroma)
    int channel;

    /// Default constructor
    Sharpen();

    /// Constructor with initial values
    Sharpen(Keyframe new_amount, Keyframe new_radius, Keyframe new_threshold);

    /// @brief This method is required for all derived classes of EffectBase, and returns a
    /// modified openshot::Frame object
    ///
    /// The frame object is passed into this method, and a frame_number is passed in which
    /// tells the effect which settings to use from its keyframes (starting at 1).
    ///
    /// @returns The modified openshot::Frame object
    /// @param frame The frame object that needs the effect applied to it
    /// @param frame_number The frame number (starting at 1) of the effect on the timeline.
    std::shared_ptr<Frame> GetFrame(std::shared_ptr<Frame> frame, int64_t frame_number) override;
    std::shared_ptr<Frame> GetFrame(int64_t n) override
    { return GetFrame(std::make_shared<Frame>(), n); }

    /// Get and Set JSON methods
    std::string Json() const override; ///< Generate JSON string of this object
    Json::Value JsonValue() const override; ///< Generate Json::Value for this object
    void SetJson(const std::string value) override; ///< Load JSON string into this object
    void SetJsonValue(const Json::Value root) override; ///< Load Json::Value into this object

    /// Get all properties for a specific frame (perfect for a UI to display the current state
    /// of all properties at any time)
    std::string PropertiesJSON(int64_t requested_frame) const override;
};

}  // namespace openshot

#endif  // OPENSHOT_SHARPEN_EFFECT_H