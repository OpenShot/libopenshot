/**
 * @file
 * @brief Header file for the Keyframe class
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

#ifndef OPENSHOT_KEYFRAME_H
#define OPENSHOT_KEYFRAME_H

#include <iostream>
#include <math.h>
#include <assert.h>
#include <vector>
#include "Exceptions.h"
#include "Fraction.h"
#include "Coordinate.h"
#include "Point.h"
#include "Json.h"

using namespace std;
using namespace openshot;

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

		/*
		 * Because points can be added in any order, we need to reorder them
		 * in ascending order based on the point.co.X value.  This simplifies
		 * processing the curve, due to all the points going from left to right.
		 */
		void ReorderPoints();

		// Process an individual segment
		void ProcessSegment(int Segment, Point p1, Point p2);

		// create lookup table for fast factorial calculation
		void CreateFactorialTable();

		// Get a factorial for a coordinate
		double Factorial(int n);

		// Calculate the factorial function for Bernstein basis
		double Ni(int n, int i);

		// Calculate Bernstein Basis
		double Bernstein(int n, int i, double t);

	public:
		vector<Point> Points;			///< Vector of all Points
		vector<Coordinate> Values;		///< Vector of all Values (i.e. the processed coordinates from the curve)
		float Auto_Handle_Percentage;	///< Percentage the left and right handles should be adjusted to, to create a smooth curve

		/// Default constructor for the Keyframe class
		Keyframe();

		/// Constructor which sets the default point & coordinate at X=0
		Keyframe(float value);

		/// Add a new point on the key-frame.  Each point has a primary coordinate, a left handle, and a right handle.
		void AddPoint(Point p);

		/// Add a new point on the key-frame, with some defaults set (BEZIER, AUTO Handles, etc...)
		void AddPoint(float x, float y);

		/// Add a new point on the key-frame, with a specific interpolation type
		void AddPoint(float x, float y, InterpolationType interpolate);

		/// Does this keyframe contain a specific point
		bool Contains(Point p);

		/// Set the handles, used for smooth curves.  The handles are based on the surrounding points.
		void SetHandles(Point current);

		/// Get the index of a point by matching a coordinate
		int FindIndex(Point p) throw(OutOfBoundsPoint);

		/// Get the value at a specific index
		float GetValue(int index);

		/// Get the rounded INT value at a specific index
		int GetInt(int index);

		/// Get the fraction that represents how many times this value is repeated in the curve
		Fraction GetRepeatFraction(int index);

		/// Get the change in Y value (from the previous Y value)
		float GetDelta(int index);

		/// Get a point at a specific index
		Point& GetPoint(int index) throw(OutOfBoundsPoint);

		// Get the number of values (i.e. coordinates on the X axis)
		int GetLength();

		/// Get the direction of the curve at a specific index (increasing or decreasing)
		bool IsIncreasing(int index);

		/// Get and Set JSON methods
		string Json(); ///< Generate JSON string of this object
		Json::Value JsonValue(); ///< Generate Json::JsonValue for this object
		void SetJson(string value) throw(InvalidJSON); ///< Load JSON string into this object
		void SetJsonValue(Json::Value root); ///< Load Json::JsonValue into this object

		/**
		 * @brief Calculate all of the values for this keyframe.
		 *
		 * This clears any existing data in the "values" vector.  This method is automatically called
		 * by AddPoint(), so you don't typically need to call this method.
		 */
		void Process();

		/// Remove a point by matching a coordinate
		void RemovePoint(Point p) throw(OutOfBoundsPoint);

		/// Remove a point by index
		void RemovePoint(int index) throw(OutOfBoundsPoint);

		/// Replace an existing point with a new point
		void UpdatePoint(int index, Point p);

		/// Print a list of points
		void PrintPoints();

		/// Print just the Y value of the point's primary coordinate
		void PrintValues();

	};

}

#endif
