/**
 * @file
 * @brief Header file for Coordinate class
 * @author Jonathan Thomas <jonathan@openshot.org>
 *
 * @section LICENSE
 *
 * Copyright (c) 2008-2014 OpenShot Studios, LLC
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

using namespace std;

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
	private:
		bool increasing; ///< Is the Y value increasing or decreasing?
		Fraction repeated; ///< Fraction of repeated Y values (for example, 1/3 would be the first Y value of 3 repeated values)
		double delta; ///< This difference in Y value (from the previous unique Y value)

	public:
		double X; ///< The X value of the coordinate (usually representing the frame #)
		double Y; ///< The Y value of the coordinate (usually representing the value of the property being animated)

		/// The default constructor, which defaults to (0,0)
		Coordinate();

		/// @brief Constructor which also sets the X and Y
		/// @param x The X coordinate (usually representing the frame #)
		/// @param y The Y coordinate (usually representing the value of the property being animated)
		Coordinate(double x, double y);

		/// @brief Set the repeating Fraction (used internally on the timeline, to track changes to coordinates)
		/// @param is_repeated The fraction representing how many times this coordinate Y value repeats (only used on the timeline)
		void Repeat(Fraction is_repeated) { repeated=is_repeated; }

		/// Get the repeating Fraction (used internally on the timeline, to track changes to coordinates)
		Fraction Repeat() { return repeated; }

		/// @brief Set the increasing flag (used internally on the timeline, to track changes to coordinates)
		/// @param is_increasing Indicates if this coorindate Y value is increasing (when compared to the previous coordinate)
		void IsIncreasing(bool is_increasing) { increasing = is_increasing; }

		/// Get the increasing flag (used internally on the timeline, to track changes to coordinates)
		bool IsIncreasing() { return increasing; }

		/// @brief Set the delta / difference between previous coordinate value (used internally on the timeline, to track changes to coordinates)
		/// @param new_delta Indicates how much this Y value differs from the previous Y value
		void Delta(double new_delta) { delta=new_delta; }

		/// Get the delta / difference between previous coordinate value (used internally on the timeline, to track changes to coordinates)
		float Delta() { return delta; }

		/// Get and Set JSON methods
		string Json(); ///< Generate JSON string of this object
		Json::Value JsonValue(); ///< Generate Json::JsonValue for this object
		void SetJson(string value) throw(InvalidJSON); ///< Load JSON string into this object
		void SetJsonValue(Json::Value root); ///< Load Json::JsonValue into this object
	};

}

#endif
