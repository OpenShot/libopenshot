/**
 * @file
 * @brief Header file for the Keyframe class
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

#ifndef OPENSHOT_KEYFRAME_H
#define OPENSHOT_KEYFRAME_H

#include <iostream>
#include <iomanip>
#include <math.h>
#include <assert.h>
#include <vector>
#include "Exceptions.h"
#include "Fraction.h"
#include "Coordinate.h"
#include "Point.h"
#include "Json.h"

namespace openshot {

	/**
	 * @brief A Keyframe is a collection of Point instances, which is used to vary a number or property over time.
	 *
	 * Keyframes are used to animate and interpolate values of properties over time.  For example, a single property
	 * can use a Keyframe instead of a constant value.  Assume you want to slide an image (from left to right) over
	 * a video.  You can create a Keyframe which will adjust the X value of the image over 100 frames (or however many
	 * frames the animation needs to last) from the value of 0 to 640.
	 *
	 * Please see the following <b>Example Code</b>:
	 * \code
	 * Keyframe k1;
	 * k1.AddPoint(Point(1,0));
	 * k1.AddPoint(Point(100,640));
	 *
	 * kf.PrintValues();
	 * \endcode
	 */
	class Keyframe {
	private:
		bool needs_update;
		double FactorialLookup[4];
		std::vector<Point> Points;			///< Vector of all Points
		std::vector<Coordinate> Values;		///< Vector of all Values (i.e. the processed coordinates from the curve)

		// Process an individual segment
		void ProcessSegment(int Segment, Point p1, Point p2);

		// create lookup table for fast factorial calculation
		void CreateFactorialTable();

		// Get a factorial for a coordinate
		double Factorial(int64_t n);

		// Calculate the factorial function for Bernstein basis
		double Ni(int64_t n, int64_t i);

		// Calculate Bernstein Basis
		double Bernstein(int64_t n, int64_t i, double t);

	public:

		/// Default constructor for the Keyframe class
		Keyframe();

		/// Constructor which sets the default point & coordinate at X=1
		Keyframe(double value);

		/// Add a new point on the key-frame.  Each point has a primary coordinate, a left handle, and a right handle.
		void AddPoint(Point p);

		/// Add a new point on the key-frame, with some defaults set (BEZIER)
		void AddPoint(double x, double y);

		/// Add a new point on the key-frame, with a specific interpolation type
		void AddPoint(double x, double y, InterpolationType interpolate);

		/// Does this keyframe contain a specific point
		bool Contains(Point p);

		/// Flip all the points in this openshot::Keyframe (useful for reversing an effect or transition, etc...)
		void FlipPoints();

		/// Get the index of a point by matching a coordinate
		int64_t FindIndex(Point p);

		/// Get the value at a specific index
		double GetValue(int64_t index);

		/// Get the rounded INT value at a specific index
		int GetInt(int64_t index);

		/// Get the rounded LONG value at a specific index
		int64_t GetLong(int64_t index);

		/// Get the fraction that represents how many times this value is repeated in the curve
		Fraction GetRepeatFraction(int64_t index);

		/// Get the change in Y value (from the previous Y value)
		double GetDelta(int64_t index);

		/// Get a point at a specific index
		Point& GetPoint(int64_t index);

		/// Get current point (or closest point to the right) from the X coordinate (i.e. the frame number)
		Point GetClosestPoint(Point p);

		/// Get current point (or closest point) from the X coordinate (i.e. the frame number)
		/// Either use the closest left point, or right point
		Point GetClosestPoint(Point p, bool useLeft);

		/// Get previous point (
		Point GetPreviousPoint(Point p);

		/// Get max point (by Y coordinate)
		Point GetMaxPoint();

		// Get the number of values (i.e. coordinates on the X axis)
		int64_t GetLength();

		/// Get the number of points (i.e. # of points)
		int64_t GetCount();

		/// Get the direction of the curve at a specific index (increasing or decreasing)
		bool IsIncreasing(int index);

		/// Get and Set JSON methods
		std::string Json(); ///< Generate JSON string of this object
		Json::Value JsonValue(); ///< Generate Json::JsonValue for this object
		void SetJson(std::string value); ///< Load JSON string into this object
		void SetJsonValue(Json::Value root); ///< Load Json::JsonValue into this object

		/**
		 * @brief Calculate all of the values for this keyframe.
		 *
		 * This clears any existing data in the "values" vector.  This method is automatically called
		 * by AddPoint(), so you don't typically need to call this method.
		 */
		void Process();

		/// Remove a point by matching a coordinate
		void RemovePoint(Point p);

		/// Remove a point by index
		void RemovePoint(int64_t index);

		/// Scale all points by a percentage (good for evenly lengthening or shortening an openshot::Keyframe)
		/// 1.0 = same size, 1.05 = 5% increase, etc...
		void ScalePoints(double scale);

		/// Replace an existing point with a new point
		void UpdatePoint(int64_t index, Point p);

		/// Print a list of points
		void PrintPoints();

		/// Print just the Y value of the point's primary coordinate
		void PrintValues();

	};

}

#endif
