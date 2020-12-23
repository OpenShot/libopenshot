/**
 * @file
 * @brief Header file for the KeyframeBase class
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

#ifndef OPENSHOT_KEYFRAMEBASE_H
#define OPENSHOT_KEYFRAMEBASE_H

#include <iostream>
#include <iomanip>
#include <cmath>
#include <assert.h>
#include <vector>
#include <string>
#include "Exceptions.h"
#include "Fraction.h"
#include "Coordinate.h"
#include "Point.h"
#include "Json.h"

namespace openshot {
    /**
	 * @brief This abstract class is the base class of all Keyframes.
	 *
	 * A Keyframe is a collection of Point instances, which is used to vary a number or property over time.
	 *
	 * Keyframes are used to animate and interpolate values of properties over time.  For example, a single property
	 * can use a Keyframe instead of a constant value.  Assume you want to slide an image (from left to right) over
	 * a video.  You can create a Keyframe which will adjust the X value of the image over 100 frames (or however many
	 * frames the animation needs to last) from the value of 0 to 640.
	 */

	/// Check if the X coordinate of a given Point is lower than a given value
	bool IsPointBeforeX(Point const & p, double const x);

	/// Linear interpolation between two points
	double InterpolateLinearCurve(Point const & left, Point const & right, double const target);

	/// Bezier interpolation between two points
	double InterpolateBezierCurve(Point const & left, Point const & right, double const target, double const allowed_error);

	/// Interpolate two points using the right Point's interpolation method
	double InterpolateBetween(Point const & left, Point const & right, double target, double allowed_error);

	// template<typename Check>
	// int64_t SearchBetweenPoints(Point const & left, Point const & right, int64_t const current, Check check);

    class KeyframeBase{
	private:
		std::string id;

    public:

        /// Blank constructor
        KeyframeBase();

		/// Default constructor
		KeyframeBase(std::string _id);

		std::string Id() const { return id; }
		void SetId(std::string _id) { id = _id; }

		/// Scale all points by a percentage (good for evenly lengthening or shortening an openshot::Keyframe)
		/// 1.0 = same size, 1.05 = 5% increase, etc...
		virtual void ScalePoints(double scale) { return; };

		/// Return the main properties of a KeyframeBBox instance using a pointer to this base class
		virtual std::map<std::string, float> GetBoxValues(int64_t frame_number) { std::map<std::string, float> ret; return ret; }; 

    };
} // Namespace openshot

#endif
