/**
 * @file
 * @brief Header file for Crop effect class
 * @author Jonathan Thomas <jonathan@openshot.org>
 *
 * @ref License
 */

// Copyright (c) 2008-2019 OpenShot Studios, LLC
//
// SPDX-License-Identifier: LGPL-3.0-or-later

#ifndef OPENSHOT_CROP_EFFECT_H
#define OPENSHOT_CROP_EFFECT_H

#include "../EffectBase.h"

#include "../Color.h"
#include "../Frame.h"
#include "../Json.h"
#include "../KeyFrame.h"

#include <memory>
#include <string>


namespace openshot
{

	/**
	 * @brief This class crops a frame image (from any side), and can be animated with
	 * openshot::Keyframe curves over time.
	 *
	 * Cropping images can be useful when wanting to remove a border around an image or video, and animating
	 * the crop can create some very interesting effects.
	 */
	class Crop : public EffectBase
	{
	private:
		/// Init effect settings
		void init_effect_details();


	public:
		Keyframe left;		///< Size of left bar
		Keyframe top;		///< Size of top bar
		Keyframe right;		///< Size of right bar
		Keyframe bottom;	///< Size of bottom bar
		Keyframe x;			///< X-offset
		Keyframe y;			///< Y-offset
		bool resize;		///< Auto-resize image after crop operation

		/// Blank constructor, useful when using Json to load the effect properties
		Crop();

		/// Default constructor, which takes 4 curves. These curves animate the crop over time.
		///
		/// @param left The curve to adjust the left bar size (between 0 and 1)
		/// @param top The curve to adjust the top bar size (between 0 and 1)
		/// @param right The curve to adjust the right bar size (between 0 and 1)
		/// @param bottom The curve to adjust the bottom bar size (between 0 and 1)
		/// @param x x-offset of original image in output frame (-1.0 - 1.0)
		/// @param y y-offset of original image in output frame (-1.0 - 1.0)
		Crop(Keyframe left, Keyframe top, Keyframe right, Keyframe bottom,
			 Keyframe x=0.0, Keyframe y=0.0);

		/// @brief This method is required for all derived classes of ClipBase, and returns a
		/// new openshot::Frame object. All Clip keyframes and effects are resolved into
		/// pixels.
		///
		/// @returns A new openshot::Frame object
		/// @param frame_number The frame number (starting at 1) of the clip or effect on the timeline.
		std::shared_ptr<openshot::Frame>
		GetFrame(int64_t frame_number) override {
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
		std::shared_ptr<openshot::Frame>
		GetFrame(std::shared_ptr<openshot::Frame> frame, int64_t frame_number) override;

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
