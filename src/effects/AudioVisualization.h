/**
 * @file
 * @brief Header file for AudioVisualization effect class
 * @author Jonathan Thomas <jonathan@openshot.org>
 *
 * @ref License
 */

// Copyright (c) 2008-2026 OpenShot Studios, LLC
//
// SPDX-License-Identifier: LGPL-3.0-or-later

#ifndef OPENSHOT_AUDIO_VISUALIZATION_EFFECT_H
#define OPENSHOT_AUDIO_VISUALIZATION_EFFECT_H

#include "../Color.h"
#include "../EffectBase.h"
#include "../Frame.h"
#include "../Json.h"
#include "../KeyFrame.h"

#include <memory>
#include <string>

namespace openshot
{
	enum AudioVisualizationType {
		AUDIO_VISUALIZATION_WAVEFORM = 0,
		AUDIO_VISUALIZATION_FILLED_WAVEFORM = 1,
		AUDIO_VISUALIZATION_BARS = 2,
		AUDIO_VISUALIZATION_RADIAL = 3,
		AUDIO_VISUALIZATION_SPECTRUM = 4,
		AUDIO_VISUALIZATION_PHASE_SCOPE = 5,
		AUDIO_VISUALIZATION_PARTICLES = 6,
		AUDIO_VISUALIZATION_VU_METER = 7,
		AUDIO_VISUALIZATION_RADIAL_BARS = 8
	};

	enum AudioVisualizationStyle {
		AUDIO_VISUALIZATION_STYLE_CLEAN = 0,
		AUDIO_VISUALIZATION_STYLE_SOFT = 1,
		AUDIO_VISUALIZATION_STYLE_NEON = 2,
		AUDIO_VISUALIZATION_STYLE_MINIMAL = 3
	};

	enum AudioVisualizationChannelLayout {
		AUDIO_VISUALIZATION_CHANNEL_AUTO = 0,
		AUDIO_VISUALIZATION_CHANNEL_COMBINED = 1,
		AUDIO_VISUALIZATION_CHANNEL_SPLIT = 2,
		AUDIO_VISUALIZATION_CHANNEL_OVERLAY = 3
	};

	enum AudioVisualizationBackground {
		AUDIO_VISUALIZATION_BACKGROUND_TRANSPARENT = 0,
		AUDIO_VISUALIZATION_BACKGROUND_SOLID = 1,
		AUDIO_VISUALIZATION_BACKGROUND_FADE = 2,
		AUDIO_VISUALIZATION_BACKGROUND_GRADIENT = 3,
		AUDIO_VISUALIZATION_BACKGROUND_SOURCE = 4
	};

	enum AudioVisualizationColorMode {
		AUDIO_VISUALIZATION_COLOR_SEED = 0,
		AUDIO_VISUALIZATION_COLOR_RAINBOW = 1
	};

	class AudioVisualization : public EffectBase
	{
	private:
		void init_effect_details();

	public:
		int visualization_type;
		int style;
		Color color;
		Keyframe intensity;
		Keyframe smoothing;
		Keyframe detail;
		Keyframe glow;
		Keyframe color_spread;
		int color_mode;
		int channel_layout;
		Keyframe frequency_low;
		Keyframe frequency_high;
		int background;

		AudioVisualization();
		AudioVisualization(int visualization_type, Color color);

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
