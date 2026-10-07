/**
 * @file
 * @brief Header file for Blur effect class
 * @author Jonathan Thomas <jonathan@openshot.org>
 *
 * @ref License
 */

// Copyright (c) 2008-2019 OpenShot Studios, LLC
//
// SPDX-License-Identifier: LGPL-3.0-or-later

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
	enum BlurMaskMode {
		BLUR_MASK_POST_BLEND = 0,
		BLUR_MASK_DRIVE_AMOUNT = 1
	};

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

		// Internal blur methods (inspired and credited to http://blog.ivank.net/fastest-gaussian-blur.html)
		void boxBlurH(unsigned char *scl, unsigned char *tcl, int w, int h, int r);
		void boxBlurT(unsigned char *scl, unsigned char *tcl, int w, int h, int r);

	protected:
		bool UseCustomMaskBlend(int64_t frame_number) const override;
		void ApplyCustomMaskBlend(std::shared_ptr<QImage> original_image, std::shared_ptr<QImage> effected_image,
							  std::shared_ptr<QImage> mask_image, int64_t frame_number) const override;

	public:
		Keyframe horizontal_radius;	///< Horizontal blur radius keyframe. The size of the horizontal blur operation in pixels.
		Keyframe vertical_radius;	///< Vertical blur radius keyframe. The size of the vertical blur operation in pixels.
		Keyframe sigma;				///< Sigma keyframe. The amount of spread in the blur operation. Should be larger than radius.
		Keyframe iterations;		///< Iterations keyframe. The # of blur iterations per pixel. 3 iterations = Gaussian.
		Keyframe left;			///< Size of left margin
		Keyframe top;			///< Size of top margin
		Keyframe right;			///< Size of right margin
		Keyframe bottom;		///< Size of bottom margin
		int mask_mode;			///< How to apply common masks to blur (post-blend or drive-amount).

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

		/// Default constructor, which takes blur curves and an affected area.
		///
		/// @param new_horizontal_radius The curve to adjust the horizontal blur radius (between 0 and 100, rounded to int)
		/// @param new_vertical_radius The curve to adjust the vertical blur radius (between 0 and 100, rounded to int)
		/// @param new_sigma The curve to adjust the sigma amount (the size of the blur brush (between 0 and 100), float values accepted)
		/// @param new_iterations The curve to adjust the # of iterations (between 1 and 100)
		/// @param new_left The curve to adjust the left margin size (between 0 and 1)
		/// @param new_top The curve to adjust the top margin size (between 0 and 1)
		/// @param new_right The curve to adjust the right margin size (between 0 and 1)
		/// @param new_bottom The curve to adjust the bottom margin size (between 0 and 1)
		Blur(Keyframe new_horizontal_radius, Keyframe new_vertical_radius, Keyframe new_sigma, Keyframe new_iterations,
			 Keyframe new_left, Keyframe new_top, Keyframe new_right, Keyframe new_bottom);

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
