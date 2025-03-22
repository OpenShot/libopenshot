/**
 * @file
 * @brief Header file for Shadow effect class
 * @author Jonathan Thomas <jonathan@openshot.org>
 * @author HaiVQ <me@haivq.com>
 *
 * @ref License
 */

// Copyright (c) 2008-2025 OpenShot Studios, LLC
//
// SPDX-License-Identifier: LGPL-3.0-or-later

#ifndef OPENSHOT_SHADOW_EFFECT_H
#define OPENSHOT_SHADOW_EFFECT_H

#include <QtWidgets/QGraphicsDropShadowEffect>
#include <QtWidgets/QGraphicsScene>
#include <QtWidgets/QGraphicsPixmapItem>
#include <QtGui/QPixmap>
#include <QtCore/QRectF>
#include <QtGui/QPainter>

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
	 * @brief This class drops shadow of image with transparent background and can be animated
	 * with openshot::Keyframe curves over time.
	 */
	class Shadow : public EffectBase
	{
	private:
		/// Init effect settings
		void init_effect_details();

	public:
		Keyframe x_offset;	///< horizontal offset of the shadow
		Keyframe y_offset;	///< vertical offset of the shadow
		Keyframe blur_radius;	///< Radius of the shadow blur
		Color color;	///< Color of the shadow

		/// Blank constructor, useful when using Json to load the effect properties
		Shadow();

		/// Default constructor, which require width, red, green, blue, alpha
		///
		/// @param x_offset The horizontal offset of the shadow (between -1000 and 1000, rounded to int)
		/// @param y_offset The vertical offset of the shadow (between -1000 and 1000, rounded to int)
		/// @param blur_radius Radius of the shadow blur (between 0 and 1000)
		/// @param color The color of the shadow
		Shadow(Keyframe x_offset, Keyframe y_offset, Keyframe blur_radius, Color color);

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
