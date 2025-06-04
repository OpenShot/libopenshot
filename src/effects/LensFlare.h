/**
* @file
 * @brief Header file for LensFlare class
 * @author Jonathan Thomas <jonathan@openshot.org>
 *
 * @ref License
 */

// Copyright (c) 2008-2025 OpenShot Studios, LLC
//
// SPDX-License-Identifier: LGPL-3.0-or-later

#ifndef OPENSHOT_LENSFLARE_EFFECT_H
#define OPENSHOT_LENSFLARE_EFFECT_H

#include "../EffectBase.h"
#include "../KeyFrame.h"
#include "../Color.h"
#include <QImage>
#include <QColor>

namespace openshot
{
    class LensFlare : public EffectBase
    {
    private:
        void init_effect_details();

    public:
        Keyframe x;
        Keyframe y;
        Keyframe brightness;
        Keyframe size;
        Keyframe spread;
        Color color;

        LensFlare();
        ~LensFlare() override;
        LensFlare(const Keyframe &xPos,
                  const Keyframe &yPos,
                  const Keyframe &intensity,
                  const Keyframe &scale,
                  const Keyframe &spreadVal,
                  const Keyframe &bladeCount,
                  const Keyframe &shapeType,
                  const Color &tint = Color("#ffffff"));

        std::shared_ptr<openshot::Frame> GetFrame(int64_t frame_number) override;
        std::shared_ptr<openshot::Frame> GetFrame(std::shared_ptr<openshot::Frame> frame,
                                                  int64_t frame_number) override;

        std::string Json() const override;
        Json::Value JsonValue() const override;
        void SetJson(const std::string value) override;
        void SetJsonValue(const Json::Value root) override;

        std::string PropertiesJSON(int64_t requested_frame) const override;
    };

} // namespace openshot

#endif // OPENSHOT_LENSFLARE_EFFECT_H
