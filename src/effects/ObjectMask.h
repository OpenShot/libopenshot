/**
 * @file
 * @brief Header file for Object Mask effect class
 * @author Jonathan Thomas <jonathan@openshot.org>
 *
 * @ref License
 */

// Copyright (c) 2026 OpenShot Studios, LLC
//
// SPDX-License-Identifier: LGPL-3.0-or-later

#ifndef OPENSHOT_OBJECT_MASK_EFFECT_H
#define OPENSHOT_OBJECT_MASK_EFFECT_H

#include "Color.h"
#include "EffectBase.h"
#include "KeyFrame.h"
#include "TrackedObjectBBox.h"

#include <map>

namespace openshot
{
    class Frame;

    struct ObjectMaskFrameData {
        int width = 0;
        int height = 0;
        std::vector<uint32_t> rle;
        BBox box;
        float score = 0.0f;

        bool HasData() const { return width > 0 && height > 0 && !rle.empty(); }
    };

    /**
     * @brief Display and expose a preprocessed segmentation mask for an object.
     */
    class ObjectMask : public EffectBase
    {
    private:
        std::string protobuf_data_path;
        std::map<size_t, ObjectMaskFrameData> masksData;

        void init_effect_details();

    public:
        Keyframe draw_mask;
        Color mask_color;
        Keyframe mask_alpha;
        Color stroke_color;
        Keyframe stroke_alpha;
        Keyframe stroke_width;

        ObjectMask();

        std::shared_ptr<Frame> GetFrame(std::shared_ptr<Frame> frame, int64_t frame_number) override;
        std::shared_ptr<openshot::Frame> GetFrame(int64_t frame_number) override { return GetFrame(std::make_shared<Frame>(), frame_number); }

        bool LoadObjMaskData(std::string inputFilePath);
        std::shared_ptr<QImage> TrackedObjectMask(std::shared_ptr<QImage> target_image, int64_t frame_number) const override;

        std::string Json() const override;
        void SetJson(const std::string value) override;
        Json::Value JsonValue() const override;
        void SetJsonValue(const Json::Value root) override;

        std::string PropertiesJSON(int64_t requested_frame) const override;
    };
}

#endif
