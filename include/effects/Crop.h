/**
 * @file
 * @brief Header file for Crop effect class
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

#ifndef OPENSHOT_CROP_EFFECT_H
#define OPENSHOT_CROP_EFFECT_H

#include "../EffectBase.h"

#include <cmath>
#include <stdio.h>
#include <memory>
#include "../Color.h"
#include "../Json.h"
#include "../KeyFrame.h"


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
		Color color;		///< Color of bars
		Keyframe left;		///< Size of left bar
		Keyframe top;		///< Size of top bar
		Keyframe right;		///< Size of right bar
		Keyframe bottom;	///< Size of bottom bar

		/// Blank constructor, useful when using Json to load the effect properties
		Crop();

		/// Default constructor, which takes 4 curves. These curves animate the crop over time.
		///
		/// @param left The curve to adjust the left bar size (between 0 and 1)
		/// @param top The curve to adjust the top bar size (between 0 and 1)
		/// @param right The curve to adjust the right bar size (between 0 and 1)
		/// @param bottom The curve to adjust the bottom bar size (between 0 and 1)
		Crop(Keyframe left, Keyframe top, Keyframe right, Keyframe bottom);

		/// @brief This method is required for all derived classes of EffectBase, and returns a
		/// modified openshot::Frame object
		///
		/// The frame object is passed into this method, and a frame_number is passed in which
		/// tells the effect which settings to use from its keyframes (starting at 1).
		///
		/// @returns The modified openshot::Frame object
		/// @param frame The frame object that needs the effect applied to it
		/// @param frame_number The frame number (starting at 1) of the effect on the timeline.
		std::shared_ptr<Frame> GetFrame(std::shared_ptr<Frame> frame, int64_t frame_number) override;

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
