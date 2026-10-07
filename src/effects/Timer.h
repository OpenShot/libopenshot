/**
 * @file
 * @brief Header file for Timer effect class
 * @author OpenShot Studios, LLC
 *
 * @ref License
 */

// Copyright (c) 2008-2026 OpenShot Studios, LLC
//
// SPDX-License-Identifier: LGPL-3.0-or-later

#ifndef OPENSHOT_TIMER_EFFECT_H
#define OPENSHOT_TIMER_EFFECT_H

#include "../Color.h"
#include "../EffectBase.h"
#include "../Enums.h"
#include "../Frame.h"
#include "../Json.h"
#include "../KeyFrame.h"

#include <memory>
#include <string>

namespace openshot
{
	enum TimerMode {
		TIMER_MODE_COUNT_UP = 0,
		TIMER_MODE_COUNT_DOWN = 1,
		TIMER_MODE_CLOCK = 2,
		TIMER_MODE_TIMECODE = 3,
		TIMER_MODE_FRAME_NUMBER = 4
	};

	enum TimerTimeSource {
		TIMER_TIME_CLIP = 0,
		TIMER_TIME_SOURCE = 1
	};

	enum TimerFormat {
		TIMER_FORMAT_MM_SS = 0,
		TIMER_FORMAT_HH_MM_SS = 1,
		TIMER_FORMAT_HH_MM_SS_MILLISECONDS = 2,
		TIMER_FORMAT_TIMECODE = 3,
		TIMER_FORMAT_FRAMES = 4
	};

	class Timer : public EffectBase
	{
	private:
		void init_effect_details();
		double ResolveFps() const;
		int64_t EffectiveFrameNumber(int64_t frame_number) const;
		double CountdownDuration(int64_t frame_number) const;
		std::string FormatSeconds(double seconds, double fps, bool duration_style) const;
		std::string FormatTimecode(double seconds, double fps) const;
		std::string TimerLayoutText(int64_t frame_number) const;

	public:
		int mode;
		int time_source;
		int format;
		int clamp;
		int gravity;
		int show_background;
		std::string font_name;
		std::string prefix;
		std::string suffix;
		Color color;
		Color stroke;
		Color background;
		Keyframe start_time;
		Keyframe end_time;
		Keyframe font_size;
		Keyframe font_alpha;
		Keyframe stroke_width;
		Keyframe x_offset;
		Keyframe y_offset;
		Keyframe background_alpha;
		Keyframe background_padding;
		Keyframe background_corner;

		Timer();

		std::shared_ptr<openshot::Frame> GetFrame(int64_t frame_number) override { return GetFrame(std::make_shared<openshot::Frame>(), frame_number); }
		std::shared_ptr<openshot::Frame> GetFrame(std::shared_ptr<openshot::Frame> frame, int64_t frame_number) override;

		double TimerSeconds(int64_t frame_number) const;
		std::string TimerText(int64_t frame_number) const;

		std::string Json() const override;
		void SetJson(const std::string value) override;
		Json::Value JsonValue() const override;
		void SetJsonValue(const Json::Value root) override;
		std::string PropertiesJSON(int64_t requested_frame) const override;
	};
}

#endif
