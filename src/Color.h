/**
 * @file
 * @brief Header file for Color class
 * @author Jonathan Thomas <jonathan@openshot.org>
 *
 * @ref License
 */

// Copyright (c) 2008-2019 OpenShot Studios, LLC
//
// SPDX-License-Identifier: LGPL-3.0-or-later

#ifndef OPENSHOT_COLOR_H
#define OPENSHOT_COLOR_H

#include "KeyFrame.h"
#include <QColor>

namespace openshot {

/**
 * @brief This class represents a color (used on the timeline and clips)
 *
 * Colors are represented by 4 curves, representing red, green, blue, and alpha.  The curves
 * can be used to animate colors over time.
 */
class Color{

public:
	openshot::Keyframe red; ///<Curve representing the red value (0 - 255)
	openshot::Keyframe green; ///<Curve representing the green value (0 - 255)
	openshot::Keyframe blue; ///<Curve representing the red value (0 - 255)
	openshot::Keyframe alpha; ///<Curve representing the alpha value (0 - 255)

	/// Default constructor
	Color() {};

	/// Constructor which takes a QColor
	explicit Color(QColor);

        /// Constructor which takes a hex string ("#rrggbb")
        explicit Color(std::string color_hex);
        explicit Color(const char* color_hex);

	/// Constructor which takes R,G,B,A
	Color(unsigned char Red, unsigned char Green, unsigned char Blue, unsigned char Alpha);

	/// Constructor which takes 4 existing Keyframe curves
	Color(openshot::Keyframe Red, openshot::Keyframe Green, openshot::Keyframe Blue, openshot::Keyframe Alpha);

	/// Get the HEX value of a color at a specific frame
	std::string GetColorHex(int64_t frame_number);

	// Get the RGBA values of a color at a specific frame
	std::vector<int> GetColorRGBA(int64_t frame_number);

	/// Get the distance between 2 RGB pairs. (0=identical colors, 10=very close colors, 760=very different colors)
	static long GetDistance(long R1, long G1, long B1, long R2, long G2, long B2);

	// Get and Set JSON methods
	std::string Json() const; ///< Generate JSON string of this object
	Json::Value JsonValue() const; ///< Generate Json::Value for this object
	void SetJson(const std::string value); ///< Load JSON string into this object
	void SetJsonValue(const Json::Value root); ///< Load Json::Value into this object
};

}  // namespace openshot

#endif
