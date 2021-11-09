/**
 * @file
 * @brief Header file for Parametric EQ audio effect class
 * @author
 *
 * @ref License
 */

// Copyright (c) 2008-2019 OpenShot Studios, LLC
//
// SPDX-License-Identifier: LGPL-3.0-or-later

#ifndef OPENSHOT_PARAMETRIC_EQ_AUDIO_EFFECT_H
#define OPENSHOT_PARAMETRIC_EQ_AUDIO_EFFECT_H
#define _USE_MATH_DEFINES

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
	 * @brief This class adds a equalization into the audio
	 *
	 */
	class ParametricEQ : public EffectBase
	{
	private:
		/// Init effect settings
		void init_effect_details();

	public:
		openshot::FilterType filter_type;
		Keyframe frequency;
		Keyframe q_factor;
		Keyframe gain;
		bool initialized;

		/// Blank constructor, useful when using Json to load the effect properties
		ParametricEQ();

		/// Default constructor
		///
		/// @param new_level
		ParametricEQ(openshot::FilterType new_filter_type, Keyframe new_frequency, Keyframe new_gain, Keyframe new_q_factor);

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

		class Filter : public juce::IIRFilter
		{
		public:
			void updateCoefficients (const double discrete_frequency,
									 const double q_factor,
									 const double gain,
									 const int filter_type);
		};

		juce::OwnedArray<Filter> filters;

		void updateFilters(int64_t frame_number, double sample_rate);
	};

}

#endif
