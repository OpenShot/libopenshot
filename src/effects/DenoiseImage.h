/**
 * @file
 * @brief Header file for DenoiseImage effect
 * @author Jonathan Thomas <jonathan@openshot.org>
 *
 * @ref License
 */

// Copyright (c) 2008-2026 OpenShot Studios, LLC
//
// SPDX-License-Identifier: LGPL-3.0-or-later

#ifndef OPENSHOT_DENOISE_IMAGE_EFFECT_H
#define OPENSHOT_DENOISE_IMAGE_EFFECT_H

#include "../AnimatedCurve.h"
#include "../EffectBase.h"
#include "../Frame.h"
#include "../Json.h"
#include "../KeyFrame.h"

#include <QImage>

#include <cstdint>
#include <memory>
#include <string>

namespace openshot
{
	class DenoiseImage : public EffectBase
	{
	private:
		QImage previous_input_;
		int64_t last_frame_;

		void init_effect_details();
		void reset_temporal_history();

	public:
		Keyframe strength;
		Keyframe detail;
		Keyframe temporal;
		Keyframe motion_safety;
		Keyframe color_noise;
		AnimatedCurve response_curve;

		DenoiseImage();
		DenoiseImage(Keyframe new_strength, Keyframe new_detail, Keyframe new_temporal,
		             Keyframe new_motion_safety, Keyframe new_color_noise);

		std::shared_ptr<openshot::Frame> GetFrame(int64_t frame_number) override {
			return GetFrame(std::make_shared<openshot::Frame>(), frame_number);
		}
		std::shared_ptr<openshot::Frame> GetFrame(std::shared_ptr<openshot::Frame> frame, int64_t frame_number) override;

		std::string Json() const override;
		Json::Value JsonValue() const override;
		void SetJson(const std::string value) override;
		void SetJsonValue(const Json::Value root) override;
		std::string PropertiesJSON(int64_t requested_frame) const override;
	};

}

#endif
