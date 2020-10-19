/**
 * @file
 * @brief Header file for Mask class
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

#ifndef OPENSHOT_MASK_EFFECT_H
#define OPENSHOT_MASK_EFFECT_H

#include "../EffectBase.h"

#include "../Json.h"
#include "../KeyFrame.h"

#include <string>
#include <memory>

namespace openshot
{
	// Forward declaration
	class ReaderBase;

	/**
	 * @brief This class uses the image libraries to apply alpha (or transparency) masks
	 * to any frame. It can also be animated, and used as a powerful Wipe transition.
	 *
	 * These masks / wipes can also be combined, such as a transparency mask on top of a clip, which
	 * is then wiped away with another animated version of this effect.
	 */
	class Mask : public EffectBase
	{
	private:
		ReaderBase *reader;
		std::shared_ptr<QImage> original_mask;
		bool needs_refresh;

		/// Init effect settings
		void init_effect_details();

	public:
		bool replace_image;		///< Replace the frame image with a grayscale image representing the mask. Great for debugging a mask.
		Keyframe brightness;	///< Brightness keyframe to control the wipe / mask effect. A constant value here will prevent animation.
		Keyframe contrast;		///< Contrast keyframe to control the hardness of the wipe effect / mask.

		/// Blank constructor, useful when using Json to load the effect properties
		Mask();

		/// Default constructor, which takes 2 curves and a mask image path. The mask is used to
		/// determine the alpha for each pixel (black is transparent, white is visible). The curves
		/// adjust the brightness and contrast of this file, to animate the effect.
		///
		/// @param mask_reader The reader of a grayscale mask image or video, to be used by the wipe transition
		/// @param mask_brightness The curve to adjust the brightness of the wipe's mask (between 100 and -100)
		/// @param mask_contrast The curve to adjust the contrast of the wipe's mask (3 is typical, 20 is a lot, 0 is invalid)
		Mask(ReaderBase *mask_reader, Keyframe mask_brightness, Keyframe mask_contrast);

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

		/// Get the reader object of the mask grayscale image
		ReaderBase* Reader() { return reader; };

		/// Set a new reader to be used by the mask effect (grayscale image)
		void Reader(ReaderBase *new_reader) { reader = new_reader; };
	};

}

#endif
