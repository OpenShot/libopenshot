/**
 * @file
 * @brief Header file for Brightness class
 * @author Jonathan Thomas <jonathan@openshot.org>
 *
 * @section LICENSE
 *
 * Copyright (c) 2008-2014 OpenShot Studios, LLC
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

#ifndef OPENSHOT_BRIGHTNESS_EFFECT_H
#define OPENSHOT_BRIGHTNESS_EFFECT_H

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

using namespace std;

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
		/// Constrain a color value from 0 to 255
		int constrain(int color_value);

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
		/// @param new_brightness The curve to adjust the brightness (between 100 and -100)
		/// @param new_contrast The curve to adjust the contrast (3 is typical, 20 is a lot, 0 is invalid)
		Brightness(Keyframe new_brightness, Keyframe new_contrast);

		/// @brief This method is required for all derived classes of EffectBase, and returns a
		/// modified openshot::Frame object
		///
		/// The frame object is passed into this method, and a frame_number is passed in which
		/// tells the effect which settings to use from it's keyframes (starting at 1).
		///
		/// @returns The modified openshot::Frame object
		/// @param frame The frame object that needs the effect applied to it
		/// @param frame_number The frame number (starting at 1) of the effect on the timeline.
		std::shared_ptr<Frame> GetFrame(std::shared_ptr<Frame> frame, long int frame_number);

		/// Get and Set JSON methods
		string Json(); ///< Generate JSON string of this object
		void SetJson(string value) throw(InvalidJSON); ///< Load JSON string into this object
		Json::Value JsonValue(); ///< Generate Json::JsonValue for this object
		void SetJsonValue(Json::Value root); ///< Load Json::JsonValue into this object

		/// Get all properties for a specific frame (perfect for a UI to display the current state
		/// of all properties at any time)
		string PropertiesJSON(long int requested_frame);
	};

}

#endif
