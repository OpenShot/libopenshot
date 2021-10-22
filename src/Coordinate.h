/**
 * @file
 * @brief Header file for Coordinate class
 * @author Jonathan Thomas <jonathan@openshot.org>
 *
 * @ref License
 */

// Copyright (c) 2008-2019 OpenShot Studios, LLC
//
// SPDX-License-Identifier: LGPL-3.0-or-later

#ifndef OPENSHOT_COORDINATE_H
#define OPENSHOT_COORDINATE_H

#include <iostream>
#include "Fraction.h"
#include "Json.h"

namespace openshot {

/**
 * @brief A Cartesian coordinate (X, Y) used in the Keyframe animation system.
 *
 * Animation involves the changing (i.e. interpolation) of numbers over time.
 * A series of Coordinate objects allows us to plot a specific curve or line
 * used during interpolation. In other words, it helps us control how a
 * value changes over time — whether it's increasing or decreasing
 * (the direction of the slope) and how quickly (the steepness of the curve).
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

	/// @brief Constructor which accepts a std::pair tuple for {X, Y}
	/// @param co A std::pair<double, double> tuple containing (X, Y)
	Coordinate(const std::pair<double, double>& co);

	// Get and Set JSON methods
	std::string Json() const; ///< Generate JSON string of this object
	Json::Value JsonValue() const; ///< Generate Json::Value for this object
	void SetJson(const std::string value); ///< Load JSON string into this object
	void SetJsonValue(const Json::Value root); ///< Load Json::Value into this object
};

/// Stream output operator for openshot::Coordinate
template<class charT, class traits>
std::basic_ostream<charT, traits>&
operator<<(std::basic_ostream<charT, traits>& o, const openshot::Coordinate& co) {
    std::basic_ostringstream<charT, traits> s;
    s.flags(o.flags());
    s.imbue(o.getloc());
    s.precision(o.precision());
    s << "(" << co.X << ", " << co.Y << ")";
    return o << s.str();
}

}

#endif
