/**
 * @file
 * @brief Header file for Distortion audio effect class
 * @author
 *
 * @ref License
 */

// Copyright (c) 2008-2019 OpenShot Studios, LLC
//
// SPDX-License-Identifier: LGPL-3.0-or-later

#ifndef OPENSHOT_DISTORTION_AUDIO_EFFECT_H
#define OPENSHOT_DISTORTION_AUDIO_EFFECT_H
#define _USE_MATH_DEFINES

#include "EffectBase.h"

#include "Json.h"
#include "KeyFrame.h"
#include "Enums.h"

#include <memory>
#include <string>

#include <AppConfig.h>
#include <juce_audio_basics/juce_audio_basics.h>
#include <juce_dsp/juce_dsp.h>

namespace openshot
{
	class Frame;
	/**
	 * @brief This class adds a distortion into the audio
	 *
	 */
	class Distortion : public EffectBase
	{
	private:
		/// Init effect settings
		void init_effect_details();

	public:
		openshot::DistortionType distortion_type;
		Keyframe input_gain;
		Keyframe output_gain;
		Keyframe tone;

		/// Default constructor
		Distortion();

		/// Constructor
		Distortion(openshot::DistortionType distortion_type,
		           Keyframe input_gain, Keyframe output_gain, Keyframe tone);

		std::shared_ptr<openshot::Frame> GetFrame(int64_t frame_number) override {
			return GetFrame(std::make_shared<openshot::Frame>(), frame_number);
		}

		std::shared_ptr<openshot::Frame> GetFrame(std::shared_ptr<openshot::Frame> frame, int64_t frame_number) override;

		// Get and Set JSON methods
		std::string Json() const override; ///< Generate JSON string of this object
		void SetJson(const std::string value) override; ///< Load JSON string into this object
		Json::Value JsonValue() const override; ///< Generate Json::Value for this object
		void SetJsonValue(const Json::Value root) override; ///< Load Json::Value into this object

		std::string PropertiesJSON(int64_t requested_frame) const override;

		class Filter : public juce::IIRFilter
		{
		public:
			void updateCoefficients(const double discrete_frequency, const double gain);
		};

		juce::OwnedArray<Filter> filters;

		void updateFilters(int64_t frame_number);
	};

}

#endif
