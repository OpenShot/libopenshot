/**
 * @file
 * @brief Header file for Color Shift effect class
 * @author Jonathan Thomas <jonathan@openshot.org>
 *
 * @ref License
 */

/* LICENSE
 *
 * Copyright (c) 2008-2019 OpenShot Studios, LLC
 * <http://www.openshotstudios.com/>. This file is part of
 * OpenShot Library (libopenshot), an open-source project dedicated to
 * delivering high quality video editing and animation solutions to the
 * world. For more information visit <http://www.openshot.org/>.
 *
 * OpenShot Library (libopenshot) is free software: you can redistribute it
 * and/or modify it under the terms of the GNU Lesser General Public License
 * as published by the Free Software Foundation, either version 3 of the
 * License, or (at your option) any later version.
 *
 * OpenShot Library (libopenshot) is distributed in the hope that it will be
 * useful, but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public License
 * along with OpenShot Library. If not, see <http://www.gnu.org/licenses/>.
 */

#ifndef OPENSHOT_COLOR_SHIFT_EFFECT_H
#define OPENSHOT_COLOR_SHIFT_EFFECT_H

#include "../EffectBase.h"

#include <cmath>
#include <stdio.h>
#include <memory>
#include "../Json.h"
#include "../KeyFrame.h"


namespace openshot
{

	/**
	 * @brief This class shifts the pixels of an image up, down, left, or right, and can be animated
	 * with openshot::Keyframe curves over time.
	 *
	 * Shifting pixels can be used in many interesting ways, especially when animating the movement of the pixels.
	 * The pixels wrap around the image (the pixels drop off one side and appear on the other side of the image).
	 */
	class ColorShift : public EffectBase
	{
	private:
		/// Init effect settings
		void init_effect_details();

	public:
		Keyframe red_x;	///< Shift the Red X coordinates (left or right)
		Keyframe red_y;	///< Shift the Red Y coordinates (up or down)
		Keyframe green_x;	///< Shift the Green X coordinates (left or right)
		Keyframe green_y;	///< Shift the Green Y coordinates (up or down)
		Keyframe blue_x;	///< Shift the Blue X coordinates (left or right)
		Keyframe blue_y;	///< Shift the Blue Y coordinates (up or down)
		Keyframe alpha_x;	///< Shift the Alpha X coordinates (left or right)
		Keyframe alpha_y;	///< Shift the Alpha Y coordinates (up or down)

		/// Blank constructor, useful when using Json to load the effect properties
		ColorShift();

		/// Default constructor, which takes 8 curves. The curves will shift the RGBA pixels up, down, left, or right
		///
		/// @param red_x The curve to adjust the Red x shift (between -1 and 1, percentage)
		/// @param red_y The curve to adjust the Red y shift (between -1 and 1, percentage)
		/// @param green_x The curve to adjust the Green x shift (between -1 and 1, percentage)
		/// @param green_y The curve to adjust the Green y shift (between -1 and 1, percentage)
		/// @param blue_x The curve to adjust the Blue x shift (between -1 and 1, percentage)
		/// @param blue_y The curve to adjust the Blue y shift (between -1 and 1, percentage)
		/// @param alpha_x The curve to adjust the Alpha x shift (between -1 and 1, percentage)
		/// @param alpha_y The curve to adjust the Alpha y shift (between -1 and 1, percentage)
		ColorShift(Keyframe red_x, Keyframe red_y, Keyframe green_x, Keyframe green_y, Keyframe blue_x, Keyframe blue_y, Keyframe alpha_x, Keyframe alpha_y);

		/// @brief This method is required for all derived classes of ClipBase, and returns a
		/// new openshot::Frame object. All Clip keyframes and effects are resolved into
		/// pixels.
		///
		/// @returns A new openshot::Frame object
		/// @param frame_number The frame number (starting at 1) of the clip or effect on the timeline.
		std::shared_ptr<openshot::Frame> GetFrame(int64_t frame_number) override { return GetFrame(std::shared_ptr<Frame> (new Frame()), frame_number); }

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

		/// Get and Set JSON methods
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
