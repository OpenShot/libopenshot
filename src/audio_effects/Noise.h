/**
 * @file
 * @brief Header file for Noise audio effect class
 * @author
 *
 * @ref License
 */

// Copyright (c) 2008-2019 OpenShot Studios, LLC
//
// SPDX-License-Identifier: LGPL-3.0-or-later

#ifndef OPENSHOT_NOISE_AUDIO_EFFECT_H
#define OPENSHOT_NOISE_AUDIO_EFFECT_H

#include "../EffectBase.h"

#include "../Frame.h"
#include "../Json.h"
#include "../KeyFrame.h"

#include <memory>
#include <string>
#include <random>
#include <math.h>


namespace openshot
{

	/**
	 * @brief This class adds a noise into the audio
	 *
	 */
	class Noise : public EffectBase
	{
	private:
		/// Init effect settings
		void init_effect_details();

	public:
		Keyframe level;	///< Noise level keyframe. The amount of noise inserted on the audio.

		/// Default constructor
		Noise();

		/// Constructor
		///
		/// @param level The audio default noise level (between 1 and 100)
		Noise(Keyframe level);

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
