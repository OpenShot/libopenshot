/**
 * @file
 * @brief Header file for ColorMap (LUT) effect
 * @author Jonathan Thomas <jonathan@openshot.org>
 *
 * @ref License
 */

// Copyright (c) 2008-2025 OpenShot Studios, LLC
//
// SPDX-License-Identifier: LGPL-3.0-or-later

#ifndef OPENSHOT_COLORMAP_EFFECT_H
#define OPENSHOT_COLORMAP_EFFECT_H

#include "../EffectBase.h"
#include "../Json.h"
#include "../KeyFrame.h"
#include <QString>
#include <QFile>
#include <QTextStream>
#include <vector>
#include <string>

namespace openshot
{

    /**
     * @brief Applies a 3D LUT (.cube) color transform to each frame.
     *
     * Loads a .cube file (LUT_3D_SIZE N × N × N) into memory, then for each pixel
     * uses nearest‐neighbor lookup and blends the result by keyframable per‐channel intensities.
     */
    class ColorMap : public EffectBase
    {
    private:
        std::string lut_path;             ///< Filesystem path to .cube LUT file
        int lut_size;                     ///< Dimension N of the cube (LUT_3D_SIZE)
        std::vector<float> lut_data;      ///< Flat array [N³ × 3] RGB lookup table
        bool needs_refresh;               ///< Reload LUT on next frame

        /// Populate info fields (class_name, name, description)
        void init_effect_details();

        /// Parse the .cube file into lut_size & lut_data
        void load_cube_file();

    public:
        Keyframe intensity;               ///< Overall intensity 0–1 (affects all channels)
        Keyframe intensity_r;             ///< Blend 0–1 for red channel
        Keyframe intensity_g;             ///< Blend 0–1 for green channel
        Keyframe intensity_b;             ///< Blend 0–1 for blue channel

        /// Blank constructor (used by JSON loader)
        ColorMap();

        /**
         * @brief Constructor with LUT path and per‐channel intensities
         *
         * @param path         Filesystem path to .cube file
         * @param i            Keyframe for overall intensity (0–1)
         * @param iR           Keyframe for red blend (0–1)
         * @param iG           Keyframe for green blend (0–1)
         * @param iB           Keyframe for blue blend (0–1)
         */
        ColorMap(const std::string &path,
                 const Keyframe &i = Keyframe(1.0),
                 const Keyframe &iR = Keyframe(1.0),
                 const Keyframe &iG = Keyframe(1.0),
                 const Keyframe &iB = Keyframe(1.0));

        /// Apply effect to a new frame
        std::shared_ptr<openshot::Frame>
        GetFrame(int64_t frame_number) override
        { return GetFrame(std::make_shared<openshot::Frame>(), frame_number); }

        /// Apply effect to an existing frame
        std::shared_ptr<openshot::Frame>
        GetFrame(std::shared_ptr<openshot::Frame> frame,
                 int64_t frame_number) override;

        // JSON serialization
        std::string Json() const override;
        Json::Value JsonValue() const override;
        void SetJson(const std::string value) override;
        void SetJsonValue(const Json::Value root) override;

        /// Expose properties (for UI)
        std::string PropertiesJSON(int64_t requested_frame) const override;
    };

} // namespace openshot

#endif // OPENSHOT_COLORMAP_EFFECT_H
