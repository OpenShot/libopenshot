/**
 * @file
 * @brief Header file for Color class
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

#ifndef OPENSHOT_COLOR_H
#define OPENSHOT_COLOR_H

#include "KeyFrame.h"
#include <QtGui/QColor>

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

		/// Constructor which takes a HEX color code
		Color(std::string color_hex);

		/// Constructor which takes R,G,B,A
		Color(unsigned char Red, unsigned char Green, unsigned char Blue, unsigned char Alpha);

		/// Constructor which takes 4 existing Keyframe curves
		Color(openshot::Keyframe Red, openshot::Keyframe Green, openshot::Keyframe Blue, openshot::Keyframe Alpha);

		/// Get the HEX value of a color at a specific frame
		std::string GetColorHex(int64_t frame_number);

		/// Get the distance between 2 RGB pairs. (0=identical colors, 10=very close colors, 760=very different colors)
		static long GetDistance(long R1, long G1, long B1, long R2, long G2, long B2);

		/// Get and Set JSON methods
		std::string Json() const; ///< Generate JSON string of this object
		Json::Value JsonValue() const; ///< Generate Json::Value for this object
		void SetJson(const std::string value); ///< Load JSON string into this object
		void SetJsonValue(const Json::Value root); ///< Load Json::Value into this object
	};


}

#endif
