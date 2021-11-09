/**
 * @file
 * @brief Header file for ChromaKey class
 * @author Jonathan Thomas <jonathan@openshot.org>
 *
 * @ref License
 */

// Copyright (c) 2008-2019 OpenShot Studios, LLC
//
// SPDX-License-Identifier: LGPL-3.0-or-later

#ifndef OPENSHOT_CHROMAKEY_EFFECT_H
#define OPENSHOT_CHROMAKEY_EFFECT_H

#include "../EffectBase.h"

#include "../Color.h"
#include "../Frame.h"
#include "../Exceptions.h"
#include "../KeyFrame.h"
#include "../Enums.h"

#include <memory>
#include <string>

namespace openshot
{

	/**
	 * @brief This class removes (i.e. keys out) a color (i.e. greenscreen)
	 *
	 * The greenscreen / bluescreen effect replaces matching colors in the video image with
	 * transparent pixels, revealing lower layers in the timeline.
	 */
	class ChromaKey : public EffectBase
	{
	private:
		Color color;
		Keyframe fuzz;
		Keyframe halo;
		ChromaKeyMethod method;

		/// Init effect settings
		void init_effect_details();

	public:

		/// Blank constructor, useful when using Json to load the effect properties
		ChromaKey();

		/// @brief Constructor specifying the key color, keying method and distance.
		///
		/// Standard constructor, which takes an openshot::Color object, a 'fuzz' factor,
		/// an optional halo threshold and an optional keying method.
		///
		/// The keying method determines the algorithm to use to determine the distance
		/// between the key color and the pixel color. The default keying method,
		/// CHROMAKEY_BASIC, treates each of the R,G,B values as a vector and calculates
		/// the length of the difference between those vectors.
		///
		/// Pixels that are less than "fuzz" distance from the key color are eliminated
		/// by setting their alpha values to zero.
		///
		/// If halo is non-zero, pixels that are withing the halo distance of the fuzz
		/// distance are given an alpha value that increases with the distance from the
		/// fuzz boundary.
		///
		/// Pixels that are at least as far as fuzz+halo from the key color are foreground
		/// pixels and are left intact.
		///
		/// The default method attempts to undo the premultiplication of alpha to find the original
		/// color of a pixel. The other methods take the color as is (with alpha premultiplied).
		///
		/// @param color The color to match
		/// @param fuzz The fuzz factor (or threshold)
		/// @param halo The additional threshold for halo elimination.
		/// @param method The keying method
		ChromaKey(Color color, Keyframe fuzz, Keyframe halo = 0.0, ChromaKeyMethod method = CHROMAKEY_BASIC);

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

		// Get all properties for a specific frame
		std::string PropertiesJSON(int64_t requested_frame) const override;
	};

}

#endif
