/**
 * @file
 * @brief Header file for Glow effect class
 * @author Jonathan Thomas <jonathan@openshot.org>
 *
 * @ref License
 */

// Copyright (c) 2008-2026 OpenShot Studios, LLC
//
// SPDX-License-Identifier: LGPL-3.0-or-later

#ifndef OPENSHOT_GLOW_EFFECT_H
#define OPENSHOT_GLOW_EFFECT_H

#include "../Color.h"
#include "../EffectBase.h"
#include "../Frame.h"
#include "../Json.h"
#include "../KeyFrame.h"

#include <memory>
#include <string>

namespace openshot
{
	enum GlowMode {
		GLOW_MODE_OUTER = 0,
		GLOW_MODE_INNER = 1
	};

	class Glow : public EffectBase
	{
	private:
		void init_effect_details();

	public:
		int mode;			///< Outer or inner glow mode.
		Keyframe opacity;	///< Overall glow opacity.
		Keyframe blur_radius;	///< Blur radius in pixels.
		Keyframe spread;	///< Boosts the source alpha before blur.
		Color color;		///< Glow tint color.

		Glow();
		Glow(Keyframe new_opacity, Keyframe new_blur_radius, Keyframe new_spread, Color new_color);

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
