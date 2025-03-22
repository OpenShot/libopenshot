/**
 * @file
 * @brief Header file for Outline effect class
 * @author Jonathan Thomas <jonathan@openshot.org>
 * @author HaiVQ <me@haivq.com>
 *
 * @ref License
 */

// Copyright (c) 2008-2019 OpenShot Studios, LLC
//
// SPDX-License-Identifier: LGPL-3.0-or-later

#ifndef OPENSHOT_OUTLINE_EFFECT_H
#define OPENSHOT_OUTLINE_EFFECT_H

#include <Color.h>

#include "../EffectBase.h"

#include "../Frame.h"
#include "../Json.h"
#include "../KeyFrame.h"

#include <memory>
#include <string>


namespace openshot
{

	/**
	 * @brief This class add an outline around image with transparent background and can be animated
	 * with openshot::Keyframe curves over time.
	 *
	 * Outlines can be added around any image or text, and animated over time.
	 * Idea from: https://stackoverflow.com/a/78480103
	 */
	class Outline : public EffectBase
	{
	private:
		/// Init effect settings
		void init_effect_details();

		// Moved to Frame.h
		// cv::Mat QImageToBGRACvMat(std::shared_ptr<QImage>& qimage);
		// std::shared_ptr<QImage> BGRACvMatToQImage(cv::Mat img);

	public:
		Keyframe width;	///< Width of the outline
		Color color;	///< Color of the outline

		/// Blank constructor, useful when using Json to load the effect properties
		Outline();

		/// Default constructor, which require width, red, green, blue, alpha
		///
		/// @param width The width of the outline (between 0 and 100, rounded to int)
		/// @param color The color of the outline
		Outline(Keyframe width, Color color);

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
