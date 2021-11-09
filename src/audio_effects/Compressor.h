/**
 * @file
 * @brief Header file for Compressor audio effect class
 * @author
 *
 * @ref License
 */

// Copyright (c) 2008-2019 OpenShot Studios, LLC
//
// SPDX-License-Identifier: LGPL-3.0-or-later

#ifndef OPENSHOT_COMPRESSOR_AUDIO_EFFECT_H
#define OPENSHOT_COMPRESSOR_AUDIO_EFFECT_H

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
	 * @brief This class adds a compressor into the audio
	 *
	 */
	class Compressor : public EffectBase
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

		/// Blank constructor, useful when using Json to load the effect properties
		Compressor();

		/// Default constructor
		///
		/// @param new_level The audio default Compressor level (between 1 and 100)
		Compressor(Keyframe new_threshold, Keyframe new_ratio, Keyframe new_attack, Keyframe new_release, Keyframe new_makeup_gain, Keyframe new_bypass);

		float calculateAttackOrRelease(float value);

		/// @brief This method is required for all derived classes of ClipBase, and returns a
		/// new openshot::Frame object. All Clip keyframes and effects are resolved into
		/// pixels.
		///
		/// @returns A new openshot::Frame object
		/// @param frame_number The frame number (starting at 1) of the clip or effect on the timeline.
		std::shared_ptr<openshot::Frame> GetFrame(int64_t frame_number) override {
			return GetFrame(std::make_shared<openshot::Frame>(), frame_number);
		}

		/// @brief This method is required for all derived classes of ClipBase, and returns a
		/// modified openshot::Frame object
		///
		/// The frame object is passed into this method and used as a starting point (pixels and audio).
		/// All Clip keyframes and effects are resolved into pixels.
		///
		/// @returns The modified openshot::Frame object
		/// @param frame The frame object that needs the clip or effect applied to it
		/// @param frame_number The frame number (starting at 1) of the clip or effect on the timeline.
		std::shared_ptr<openshot::Frame> GetFrame(std::shared_ptr<openshot::Frame> frame, int64_t frame_number) override;

		// Get and Set JSON methods
		std::string Json() const override; ///< Generate JSON string of this object
		void SetJson(const std::string value) override; ///< Load JSON string into this object
		Json::Value JsonValue() const override; ///< Generate Json::Value for this object
		void SetJsonValue(const Json::Value root) override; ///< Load Json::Value into this object

		/// Get all properties for a specific frame (perfect for a UI to display the current state
		/// of all properties at any time)
		std::string PropertiesJSON(int64_t requested_frame) const override;
	};

}

#endif
