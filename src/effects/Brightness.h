/**
 * @file
 * @brief Header file for Brightness class
 * @author Jonathan Thomas <jonathan@openshot.org>
 *
 * @ref License
 */

// Copyright (c) 2008-2019 OpenShot Studios, LLC
//
// SPDX-License-Identifier: LGPL-3.0-or-later

#ifndef OPENSHOT_BRIGHTNESS_EFFECT_H
#define OPENSHOT_BRIGHTNESS_EFFECT_H

#include "../EffectBase.h"

#include "../Frame.h"
#include "../Json.h"
#include "../KeyFrame.h"

#include <memory>
#include <string>

namespace openshot
{

	/**
	 * @brief This class adjusts the brightness and contrast of an image, and can be animated
	 * with openshot::Keyframe curves over time.
	 *
	 * Adjusting the brightness and contrast over time can create many different powerful effects.
	 */
	class Brightness : public EffectBase
	{
	private:
		/// Init effect settings
		void init_effect_details();

	public:
		Keyframe brightness;	///< Brightness keyframe. A constant value here will prevent animation.
		Keyframe contrast;		///< Contrast keyframe.

		/// Blank constructor, useful when using Json to load the effect properties
		Brightness();

		/// Default constructor, which takes 2 curves. The curves adjust the brightness and
		// contrast of a frame's image.
		///
		/// @param new_brightness The curve to adjust the brightness (from -1 to +1, 0 is default/"off")
		/// @param new_contrast The curve to adjust the contrast (3 is typical, 20 is a lot, 100 is max. 0 is invalid)
		Brightness(Keyframe new_brightness, Keyframe new_contrast);

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
