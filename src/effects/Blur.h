/**
 * @file
 * @brief Header file for Blur effect class
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

#ifndef OPENSHOT_BLUR_EFFECT_H
#define OPENSHOT_BLUR_EFFECT_H

#include "../EffectBase.h"

#include "../Frame.h"
#include "../Json.h"
#include "../KeyFrame.h"

#include <memory>
#include <string>

namespace openshot
{

	/**
	 * @brief This class adjusts the blur of an image, and can be animated
	 * with openshot::Keyframe curves over time.
	 *
	 * Adjusting the blur of an image over time can create many different powerful effects. To achieve a
	 * box blur effect, use identical horizontal and vertical blur values. To achieve a Gaussian blur,
	 * use 3 iterations, a sigma of 3.0, and a radius between 3 and X (depending on how much blur you want).
	 */
	class Blur : public EffectBase
	{
	private:
		/// Init effect settings
		void init_effect_details();

		/// Internal blur methods (inspired and credited to http://blog.ivank.net/fastest-gaussian-blur.html)
		void boxBlurH(unsigned char *scl, unsigned char *tcl, int w, int h, int r);
		void boxBlurT(unsigned char *scl, unsigned char *tcl, int w, int h, int r);


	public:
		Keyframe horizontal_radius;	///< Horizontal blur radius keyframe. The size of the horizontal blur operation in pixels.
		Keyframe vertical_radius;	///< Vertical blur radius keyframe. The size of the vertical blur operation in pixels.
		Keyframe sigma;				///< Sigma keyframe. The amount of spread in the blur operation. Should be larger than radius.
		Keyframe iterations;		///< Iterations keyframe. The # of blur iterations per pixel. 3 iterations = Gaussian.

		/// Blank constructor, useful when using Json to load the effect properties
		Blur();

		/// Default constructor, which takes 1 curve. The curve adjusts the blur radius
		/// of a frame's image.
		///
		/// @param new_horizontal_radius The curve to adjust the horizontal blur radius (between 0 and 100, rounded to int)
		/// @param new_vertical_radius The curve to adjust the vertical blur radius (between 0 and 100, rounded to int)
		/// @param new_sigma The curve to adjust the sigma amount (the size of the blur brush (between 0 and 100), float values accepted)
		/// @param new_iterations The curve to adjust the # of iterations (between 1 and 100)
		Blur(Keyframe new_horizontal_radius, Keyframe new_vertical_radius, Keyframe new_sigma, Keyframe new_iterations);

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
