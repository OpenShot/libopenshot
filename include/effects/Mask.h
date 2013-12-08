/**
 * @file
 * @brief Header file for Mask class
 * @author Jonathan Thomas <jonathan@openshot.org>
 *
 * @section LICENSE
 *
 * Copyright (c) 2008-2013 OpenShot Studios, LLC
 * (http://www.openshotstudios.com). This file is part of
 * OpenShot Library (http://www.openshot.org), an open-source project
 * dedicated to delivering high quality video editing and animation solutions
 * to the world.
 *
 * OpenShot Library is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * OpenShot Library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with OpenShot Library.  If not, see <http://www.gnu.org/licenses/>.
 */

#ifndef OPENSHOT_WIPE_EFFECT_H
#define OPENSHOT_WIPE_EFFECT_H

#include "../EffectBase.h"

#include <cmath>
#include <ctime>
#include <iostream>
#include <omp.h>
#include <stdio.h>
#include <tr1/memory>
#include "Magick++.h"
#include "../Color.h"
#include "../Exceptions.h"
#include "../Json.h"
#include "../KeyFrame.h"
#include "../ReaderBase.h"

using namespace std;

namespace openshot
{

	/**
	 * @brief This class uses the ImageMagick++ libraries, to apply alpha (or transparency) masks
	 * to any frame. It can also be animated, and used as a powerful Wipe transition.
	 *
	 * These masks / wipes can also be combined, such as a transparency mask on top of a clip, which
	 * is then wiped away with another animated version of this effect.
	 */
	class Mask : public EffectBase
	{
	private:
		ReaderBase *reader;
		Keyframe brightness;
		Keyframe contrast;
		tr1::shared_ptr<Magick::Image> mask;

		/// Set brightness and contrast
		void set_brightness_and_contrast(tr1::shared_ptr<Magick::Image> image, float brightness, float contrast);

	public:

		/// Default constructor, which takes 2 curves and a mask image path. The mask is used to
		/// determine the alpha for each pixel (black is transparent, white is visible). The curves
		/// adjust the brightness and contrast of this file, to animate the effect.
		///
		/// @param mask_reader The reader of a grayscale mask image or video, to be used by the wipe transition
		/// @param mask_brightness The curve to adjust the brightness of the wipe's mask
		/// @param mask_contrast The curve to adjust the contrast of the wipe's mask
		Mask(ReaderBase *mask_reader, Keyframe mask_brightness, Keyframe mask_contrast) throw(InvalidFile, ReaderClosed);

		/// @brief This method is required for all derived classes of EffectBase, and returns a
		/// modified openshot::Frame object
		///
		/// The frame object is passed into this method, and a frame_number is passed in which
		/// tells the effect which settings to use from it's keyframes (starting at 1).
		///
		/// @returns The modified openshot::Frame object
		/// @param frame The frame object that needs the effect applied to it
		/// @param frame_number The frame number (starting at 1) of the effect on the timeline.
		tr1::shared_ptr<Frame> GetFrame(tr1::shared_ptr<Frame> frame, int frame_number);

		/// Get and Set JSON methods
		string Json(); ///< Generate JSON string of this object
		void SetJson(string value) throw(InvalidJSON); ///< Load JSON string into this object
		Json::Value JsonValue(); ///< Generate Json::JsonValue for this object
		void SetJsonValue(Json::Value root); ///< Load Json::JsonValue into this object
	};

}

#endif
