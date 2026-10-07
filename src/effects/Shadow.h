/**
 * @file
 * @brief Header file for Shadow effect class
 * @author Jonathan Thomas <jonathan@openshot.org>
 *
 * @ref License
 */

// Copyright (c) 2008-2026 OpenShot Studios, LLC
//
// SPDX-License-Identifier: LGPL-3.0-or-later

#ifndef OPENSHOT_SHADOW_EFFECT_H
#define OPENSHOT_SHADOW_EFFECT_H

#include "../Color.h"
#include "../EffectBase.h"
#include "../Frame.h"
#include "../Json.h"
#include "../KeyFrame.h"

#include <memory>
#include <string>

namespace openshot
{
	class Shadow : public EffectBase
	{
	private:
		void init_effect_details();

	public:
		Keyframe opacity;	///< Overall shadow opacity.
		Keyframe blur_radius;	///< Blur radius in pixels.
		Keyframe spread;	///< Boosts the source alpha before blur.
		Keyframe distance;	///< Shadow offset distance in pixels.
		Keyframe angle;		///< Shadow angle in degrees.
		Color color;		///< Shadow tint color.

		Shadow();
		Shadow(Keyframe new_opacity, Keyframe new_blur_radius, Keyframe new_spread,
			   Keyframe new_distance, Keyframe new_angle, Color new_color);

		std::shared_ptr<openshot::Frame> GetFrame(int64_t frame_number) override {
			return GetFrame(std::make_shared<openshot::Frame>(), frame_number);
		}

		std::shared_ptr<openshot::Frame> GetFrame(std::shared_ptr<openshot::Frame> frame,
												  int64_t frame_number) override;

		std::string Json() const override;
		void SetJson(const std::string value) override;
		Json::Value JsonValue() const override;
		void SetJsonValue(const Json::Value root) override;

		std::string PropertiesJSON(int64_t requested_frame) const override;
	};
}

#endif
