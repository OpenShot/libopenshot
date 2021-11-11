/**
 * @file
 * @brief Header file for Expander audio effect class
 * @author
 *
 * @ref License
 */

// Copyright (c) 2008-2019 OpenShot Studios, LLC
//
// SPDX-License-Identifier: LGPL-3.0-or-later

#ifndef OPENSHOT_EXPANDER_AUDIO_EFFECT_H
#define OPENSHOT_EXPANDER_AUDIO_EFFECT_H

#include "EffectBase.h"

#include "Json.h"
#include "KeyFrame.h"
#include "Enums.h"

#include <memory>
#include <string>

#include <AppConfig.h>
#include <juce_audio_basics/juce_audio_basics.h>

namespace openshot
{
	class Frame;

	/**
	 * @brief This class adds a expander (or noise gate) into the audio
	 *
	 */
	class Expander : public EffectBase
	{
	private:
		/// Init effect settings
		void init_effect_details();


	public:
		Keyframe threshold;
		Keyframe ratio;
		Keyframe attack;
		Keyframe release;
		Keyframe makeup_gain;
		Keyframe bypass;

		juce::AudioBuffer<float> mixed_down_input;
		float xl;
		float yl;
		float xg;
		float yg;
		float control;

		float input_level;
		float yl_prev;

		float inverse_sample_rate;
		float inverseE;

		/// Default constructor
		Expander();

		/// Constructor
		Expander(Keyframe threshold, Keyframe ratio, Keyframe attack,
		         Keyframe release, Keyframe makeup_gain, Keyframe bypass);

		float calculateAttackOrRelease(float value);

		std::shared_ptr<openshot::Frame> GetFrame(int64_t frame_number) override {
			return GetFrame(std::make_shared<openshot::Frame>(), frame_number);
		}

		std::shared_ptr<openshot::Frame>
		GetFrame(std::shared_ptr<openshot::Frame> frame, int64_t frame_number) override;

		// Get and Set JSON methods
		std::string Json() const override; ///< Generate JSON string of this object
		void SetJson(const std::string value) override; ///< Load JSON string into this object
		Json::Value JsonValue() const override; ///< Generate Json::Value for this object
		void SetJsonValue(const Json::Value root) override; ///< Load Json::Value into this object

		std::string PropertiesJSON(int64_t requested_frame) const override;
	};

}

#endif
