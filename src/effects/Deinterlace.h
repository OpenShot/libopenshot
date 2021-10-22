/**
 * @file
 * @brief Header file for De-interlace class
 * @author Jonathan Thomas <jonathan@openshot.org>
 *
 * @ref License
 */

// Copyright (c) 2008-2019 OpenShot Studios, LLC
//
// SPDX-License-Identifier: LGPL-3.0-or-later

#ifndef OPENSHOT_DEINTERLACE_EFFECT_H
#define OPENSHOT_DEINTERLACE_EFFECT_H

#include "../EffectBase.h"

#include <string>
#include <memory>
#include "../Frame.h"
#include "../Json.h"
#include "../KeyFrame.h"

namespace openshot
{

	/**
	 * @brief This class uses the ImageMagick++ libraries, to de-interlace the image, which
	 * removes the EVEN or ODD horizontal lines (which represent different points of time).
	 *
	 * This is most useful when converting video made for traditional TVs to computers,
	 * which are not interlaced.
	 */
	class Deinterlace : public EffectBase
	{
	private:
		bool isOdd;

		/// Init effect settings
		void init_effect_details();

	public:

		/// Default constructor, useful when using Json to load the effect properties
		Deinterlace();

		/// Constructor
		Deinterlace(bool isOdd);

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
