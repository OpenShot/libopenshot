/**
 * @file
 * @brief Header file for Delay audio effect class
 * @author
 *
 * @ref License
 */

// Copyright (c) 2008-2019 OpenShot Studios, LLC
//
// SPDX-License-Identifier: LGPL-3.0-or-later

#ifndef OPENSHOT_DELAY_AUDIO_EFFECT_H
#define OPENSHOT_DELAY_AUDIO_EFFECT_H

#include "EffectBase.h"

#include "Json.h"
#include "KeyFrame.h"

#include <memory>
#include <string>

#include <AppConfig.h>
#include <juce_audio_basics/juce_audio_basics.h>

namespace openshot
{
	class Frame;

	/**
	 * @brief This class adds a delay into the audio
	 *
	 */
	class Delay : public EffectBase
	{
	private:
		/// Init effect settings
		void init_effect_details();

	public:
		Keyframe delay_time;

		juce::AudioBuffer<float> delay_buffer;
		int delay_buffer_samples;
		int delay_buffer_channels;
		int delay_write_position;
		bool initialized;

		/// Default constructor
		Delay();

		/// Constructor
		Delay(Keyframe new_delay_time);

		std::shared_ptr<openshot::Frame> GetFrame(int64_t frame_number) override {
			return GetFrame(std::make_shared<openshot::Frame>(), frame_number);
		}

		void setup(std::shared_ptr<openshot::Frame> frame);

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
