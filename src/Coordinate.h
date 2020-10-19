/**
 * @file
 * @brief Header file for Coordinate class
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

#ifndef OPENSHOT_COORDINATE_H
#define OPENSHOT_COORDINATE_H

#include <iostream>
#include "Exceptions.h"
#include "Fraction.h"
#include "Json.h"

namespace openshot {

	/**
	 * @brief This class represents a Cartesian coordinate (X, Y) used in the Keyframe animation system.
	 *
	 * Animation involves the changing (i.e. interpolation) of numbers over time.  A series of Coordinate
	 * objects allows us to plot a specific curve or line used during interpolation.  In other words, it helps us
	 * control how a number changes over time (quickly or slowly).
	 *
	 * Please see the following <b>Example Code</b>:
	 * \code
	 * Coordinate c1(2,4);
	 * assert(c1.X == 2.0f);
	 * assert(c1.Y == 4.0f);
	 * \endcode
	 */
	class Coordinate {
	public:
		double X; ///< The X value of the coordinate (usually representing the frame #)
		double Y; ///< The Y value of the coordinate (usually representing the value of the property being animated)

		/// The default constructor, which defaults to (0,0)
		Coordinate();

		/// @brief Constructor which also sets the X and Y
		/// @param x The X coordinate (usually representing the frame #)
		/// @param y The Y coordinate (usually representing the value of the property being animated)
		Coordinate(double x, double y);

		/// Get and Set JSON methods
		std::string Json() const; ///< Generate JSON string of this object
		Json::Value JsonValue() const; ///< Generate Json::Value for this object
		void SetJson(const std::string value); ///< Load JSON string into this object
		void SetJsonValue(const Json::Value root); ///< Load Json::Value into this object
	};

}

#endif
