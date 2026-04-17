/**
 * @file
 * @brief Header file for Displace effect class
 * @author Jonathan Thomas <jonathan@openshot.org>
 *
 * @ref License
 */

// Copyright (c) 2008-2026 OpenShot Studios, LLC
//
// SPDX-License-Identifier: LGPL-3.0-or-later

#ifndef OPENSHOT_DISPLACE_EFFECT_H
#define OPENSHOT_DISPLACE_EFFECT_H

#include "../EffectBase.h"

#include "../Frame.h"
#include "../Json.h"
#include "../KeyFrame.h"

#include <memory>
#include <string>

namespace openshot
{
	// Forward declaration
	class ReaderBase;

	/**
	 * @brief This class uses a grayscale image or video to displace frame pixels.
	 *
	 * A displacement map is sampled per pixel, then used to offset the source pixel
	 * lookup in the X and Y directions. Mid-gray represents no movement, while
	 * darker and lighter values move pixels in opposite directions.
	 */
	class Displace : public EffectBase
	{
	private:
		ReaderBase* map_reader = nullptr; ///< Reader for the displacement map image/video.
		std::shared_ptr<QImage> cached_single_map_image; ///< Cached scaled map for still-image map sources.
		int cached_single_map_width = 0; ///< Cached map width.
		int cached_single_map_height = 0; ///< Cached map height.

		/// Init effect settings
		void init_effect_details();

		/// Resolve a scaled displacement map image for the target frame dimensions.
		std::shared_ptr<QImage> GetMapImage(std::shared_ptr<QImage> target_image, int64_t frame_number);

	public:
		bool replace_image;	///< Replace the frame image with the processed displacement map for debugging.
		bool invert;	///< Invert the displacement map before converting it to offsets.
		Keyframe strength;	///< Overall strength multiplier for the displacement effect.
		Keyframe horizontal;	///< Horizontal displacement amount as a percentage of image width.
		Keyframe vertical;	///< Vertical displacement amount as a percentage of image height.
		Keyframe brightness;	///< Brightness adjustment for the displacement map.
		Keyframe contrast;	///< Contrast adjustment for the displacement map.

		/// Blank constructor, useful when using Json to load the effect properties
		Displace();

		/// Default constructor, which takes displacement curves and a map image path.
		///
		/// @param map_reader The reader of a grayscale image or video used to drive displacement
		/// @param map_strength The curve to adjust the overall displacement strength
		/// @param map_horizontal The curve to adjust horizontal displacement percentage
		/// @param map_vertical The curve to adjust vertical displacement percentage
		/// @param map_brightness The curve to adjust the displacement map brightness
		/// @param map_contrast The curve to adjust the displacement map contrast
		Displace(ReaderBase *map_reader, Keyframe map_strength, Keyframe map_horizontal,
				 Keyframe map_vertical, Keyframe map_brightness, Keyframe map_contrast);

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
		/// @returns The modified frame object
		/// @param frame The frame object that needs the effect applied to it
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

		/// Get the reader object of the displacement map
		ReaderBase* Reader() { return map_reader; };

		/// Set a new reader to be used by the displacement effect
		void Reader(ReaderBase *new_reader);

		~Displace() override;
	};

}

#endif
