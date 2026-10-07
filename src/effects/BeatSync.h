/**
 * @file
 * @brief Header file for BeatSync effect class
 * @author Jonathan Thomas <jonathan@openshot.org>
 *
 * @ref License
 */

// Copyright (c) 2008-2026 OpenShot Studios, LLC
//
// SPDX-License-Identifier: LGPL-3.0-or-later

#ifndef OPENSHOT_BEAT_SYNC_EFFECT_H
#define OPENSHOT_BEAT_SYNC_EFFECT_H

#include "../AnimatedCurve.h"
#include "../Color.h"
#include "../EffectBase.h"
#include "../Frame.h"
#include "../Json.h"
#include "../KeyFrame.h"

#include <memory>
#include <string>

namespace openshot
{
	class BeatSync : public EffectBase
	{
	private:
		void init_effect_details();
		float envelope_;         // persistent IIR envelope state
		int64_t last_frame_;     // detects seeks so we can reset envelope

	public:
		Color low_color;         // generated color at minimum response
		Color high_color;        // generated color at maximum response
		Keyframe intensity;      // audio gain multiplier (0.0–10.0)
		Keyframe threshold;      // energy floor below which nothing happens (0.0–1.0)
		Keyframe attack_ms;      // envelope rise time in milliseconds
		Keyframe decay_ms;       // envelope fall time in milliseconds
		Keyframe frequency_low;  // normalized frequency floor (0=20Hz, 1=20kHz)
		Keyframe frequency_high; // normalized frequency ceiling (0=20Hz, 1=20kHz)
		bool invert;             // flip: silence = max effect, loud = no effect
		AnimatedCurve response_curve; // maps normalized audio response to color blend

		BeatSync();
		BeatSync(Color color, Keyframe intensity);

		std::shared_ptr<openshot::Frame> GetFrame(int64_t frame_number) override { return GetFrame(std::make_shared<openshot::Frame>(), frame_number); }
		std::shared_ptr<openshot::Frame> GetFrame(std::shared_ptr<openshot::Frame> frame, int64_t frame_number) override;

		std::string Json() const override;
		void SetJson(const std::string value) override;
		Json::Value JsonValue() const override;
		void SetJsonValue(const Json::Value root) override;
		std::string PropertiesJSON(int64_t requested_frame) const override;
	};
}

#endif
