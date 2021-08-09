/**
 * @file
 * @brief Header file for Hue effect class
 * @author Jonathan Thomas <jonathan@openshot.org>
 *
 * @ref License
 */

// Copyright (c) 2008-2019 OpenShot Studios, LLC
//
// SPDX-License-Identifier: LGPL-3.0-or-later

#ifndef OPENSHOT_HUE_EFFECT_H
#define OPENSHOT_HUE_EFFECT_H

#include "../EffectBase.h"

#include "../Frame.h"
#include "../Json.h"
#include "../KeyFrame.h"

#include <string>
#include <memory>


namespace openshot
{

	/**
	 * @brief This class shifts the hue of an image, and can be animated with openshot::Keyframe curves over time.
	 *
	 * Shifting hue can adjust the colors in an image towards red, blue, green, or anywhere in between. Animating hue
	 * can create some fun and interesting effects, but can also be used to change the mood of a scene, etc...
	 */
	class Hue : public EffectBase
	{
	private:
		/// Init effect settings
		void init_effect_details();


	public:
		Keyframe hue;	///< Shift the hue coordinates (left or right)

		/// Default constructor, useful when using Json to load the effect properties
		Hue();

		/// Constructor which takes 1 curve. The curves will shift the hue of the image.
		///
		/// @param hue The curve to adjust the hue shift (between 0 and 1)
		Hue(Keyframe hue);

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
