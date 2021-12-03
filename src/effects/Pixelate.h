/**
 * @file
 * @brief Header file for Pixelate effect class
 * @author Jonathan Thomas <jonathan@openshot.org>
 *
 * @ref License
 */

// Copyright (c) 2008-2019 OpenShot Studios, LLC
//
// SPDX-License-Identifier: LGPL-3.0-or-later

#ifndef OPENSHOT_PIXELATE_EFFECT_H
#define OPENSHOT_PIXELATE_EFFECT_H

#include "../EffectBase.h"

#include "../Frame.h"
#include "../Json.h"
#include "../KeyFrame.h"

#include <memory>
#include <string>

namespace openshot
{

	/**
	 * @brief This class pixelates an image, and can be animated with openshot::Keyframe curves over time.
	 *
	 * Pixelating the image is the process of increasing the size of visible pixels, thus loosing visual
	 * clarity of the image. The area to pixelate can be set and animated with keyframes also.
	 */
	class Pixelate : public EffectBase
	{
	private:
		/// Init effect settings
		void init_effect_details();


	public:
		Keyframe pixelization;	///< Amount of pixelization
		Keyframe left;			///< Size of left margin
		Keyframe top;			///< Size of top margin
		Keyframe right;			///< Size of right margin
		Keyframe bottom;		///< Size of bottom margin

		/// Default constructor, useful when using Json to load the effect properties
		Pixelate();

		/// Cnstructor which takes 5 curves. These curves animate the pixelization effect over time.
		///
		/// @param pixelization The curve to adjust the amount of pixelization (0 to 1)
		/// @param left The curve to adjust the left margin size (between 0 and 1)
		/// @param top The curve to adjust the top margin size (between 0 and 1)
		/// @param right The curve to adjust the right margin size (between 0 and 1)
		/// @param bottom The curve to adjust the bottom margin size (between 0 and 1)
		Pixelate(Keyframe pixelization, Keyframe left, Keyframe top, Keyframe right, Keyframe bottom);

		/// @brief This method is required for all derived classes of ClipBase, and returns a
		/// new openshot::Frame object. All Clip keyframes and effects are resolved into
		/// pixels.
		///
		/// @returns A new openshot::Frame object
		/// @param frame_number The frame number (starting at 1) of the clip or effect on the timeline.
		std::shared_ptr<openshot::Frame>
		GetFrame(int64_t frame_number) override {
			return GetFrame(std::make_shared<openshot::Frame>(),
			                frame_number);
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
