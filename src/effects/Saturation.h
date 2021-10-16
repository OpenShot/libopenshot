/**
 * @file
 * @brief Header file for Saturation class
 * @author Jonathan Thomas <jonathan@openshot.org>
 *
 * @ref License
 */

// Copyright (c) 2008-2019 OpenShot Studios, LLC
//
// SPDX-License-Identifier: LGPL-3.0-or-later

#ifndef OPENSHOT_SATURATION_EFFECT_H
#define OPENSHOT_SATURATION_EFFECT_H

#include "../EffectBase.h"

#include <cmath>
#include <ctime>
#include <iostream>
#include <omp.h>
#include <stdio.h>
#include <memory>
#include "../Color.h"
#include "../Exceptions.h"
#include "../Json.h"
#include "../KeyFrame.h"
#include "../ReaderBase.h"
#include "../FFmpegReader.h"
#include "../QtImageReader.h"
#include "../ChunkReader.h"

namespace openshot
{

	/**
	 * @brief This class adjusts the saturation of color on a frame's image.
	 *
	 * This can be animated by passing in a Keyframe. Animating the color saturation can create
	 * some very cool effects.
	 */
	class Saturation : public EffectBase
	{
	private:
		/// Init effect settings
		void init_effect_details();

	public:
		Keyframe saturation;	///< Overall color saturation: 0.0 = greyscale, 1.0 = normal, 2.0 = double saturation
		Keyframe saturation_R;	///< Red color saturation
		Keyframe saturation_G;	///< Green color saturation
		Keyframe saturation_B;	///< Blue color saturation

		/// Blank constructor, useful when using Json to load the effect properties
		Saturation();

		/// Default constructor, which takes four curves (one common curve and one curve per color), to adjust the color saturation over time.
		///
		/// @param saturation The curve to adjust the saturation of the frame's image (0.0 = greyscale, 1.0 = normal, 2.0 = double saturation)
		/// @param saturation_R The curve to adjust red saturation of the frame's image (0.0 = greyscale, 1.0 = normal, 2.0 = double saturation)
		/// @param saturation_G The curve to adjust green saturation of the frame's image (0.0 = greyscale, 1.0 = normal, 2.0 = double saturation)
		/// @param saturation_B The curve to adjust blue saturation of the frame's image (0.0 = greyscale, 1.0 = normal, 2.0 = double saturation)
		Saturation(Keyframe saturation, Keyframe saturation_R, Keyframe saturation_G, Keyframe saturation_B);

		/// @brief This method is required for all derived classes of ClipBase, and returns a
		/// new openshot::Frame object. All Clip keyframes and effects are resolved into
		/// pixels.
		///
		/// @returns A new openshot::Frame object
		/// @param frame_number The frame number (starting at 1) of the clip or effect on the timeline.
		std::shared_ptr<openshot::Frame> GetFrame(int64_t frame_number) override { return GetFrame(std::make_shared<openshot::Frame>(), frame_number); }

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
