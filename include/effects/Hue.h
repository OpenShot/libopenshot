/**
 * @file
 * @brief Header file for Hue effect class
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

#ifndef OPENSHOT_HUE_EFFECT_H
#define OPENSHOT_HUE_EFFECT_H

#include "../EffectBase.h"

#include <cmath>
#include <stdio.h>
#include <memory>
#include "../Json.h"
#include "../KeyFrame.h"


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

		/// Blank constructor, useful when using Json to load the effect properties
		Hue();

		/// Default constructor, which takes 1 curve. The curves will shift the hue of the image.
		///
		/// @param hue The curve to adjust the hue shift (between 0 and 1)
		Hue(Keyframe hue);

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
