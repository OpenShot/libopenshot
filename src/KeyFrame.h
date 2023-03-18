/**
 * @file
 * @brief Header file for the Keyframe class
 * @author Jonathan Thomas <jonathan@openshot.org>
 *
 * @ref License
 */

// Copyright (c) 2008-2019 OpenShot Studios, LLC
//
// SPDX-License-Identifier: LGPL-3.0-or-later

#ifndef OPENSHOT_KEYFRAME_H
#define OPENSHOT_KEYFRAME_H

#include <iostream>
#include <vector>

#include "Point.h"
#include "Json.h"

namespace openshot {

	/// Check if the X coordinate of a given Point is lower than a given value
	bool IsPointBeforeX(Point const & p, double const x);

	/// Linear interpolation between two points
	double InterpolateLinearCurve(Point const & left, Point const & right, double const target);

	/// Bezier interpolation between two points
	double InterpolateBezierCurve(Point const & left, Point const & right, double const target, double const allowed_error);

	/// Interpolate two points using the right Point's interpolation method
	double InterpolateBetween(Point const & left, Point const & right, double target, double allowed_error);

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
		std::vector<Point> Points;	///< Vector of all Points

	public:
		/// Default constructor for the Keyframe class
		Keyframe() = default;

		/// Constructor which sets the default point & coordinate at X=1
		Keyframe(double value);

		/// Constructor which adds a supplied vector of Points
		Keyframe(const std::vector<openshot::Point>& points);

		/// Destructor
		~Keyframe();

		/// Add a new point on the key-frame.  Each point has a primary coordinate, a left handle, and a right handle.
		void AddPoint(Point p);

		/// Add a new point on the key-frame, with optional interpolation type
		void AddPoint(double x, double y, InterpolationType interpolate=BEZIER);

		/// Does this keyframe contain a specific point
		bool Contains(Point p) const;

		/// Flip all the points in this openshot::Keyframe (useful for reversing an effect or transition, etc...)
		void FlipPoints();

		/// Get the index of a point by matching a coordinate
		int64_t FindIndex(Point p) const;

		/// Get the value at a specific index
		double GetValue(int64_t index) const;

		/// Get the rounded INT value at a specific index
		int GetInt(int64_t index) const;

		/// Get the rounded LONG value at a specific index
		int64_t GetLong(int64_t index) const;

		/// Get the change in Y value (from the previous Y value)
		double GetDelta(int64_t index) const;

		/// Get a point at a specific index
		Point const & GetPoint(int64_t index) const;

		/// Get current point (or closest point to the right) from the X coordinate (i.e. the frame number)
		Point GetClosestPoint(Point p) const;

		/// Get current point (or closest point) from the X coordinate (i.e. the frame number)
		/// Either use the closest left point, or right point
		Point GetClosestPoint(Point p, bool useLeft) const;

		/// Get previous point (
		Point GetPreviousPoint(Point p) const;

		/// Get max point (by Y coordinate)
		Point GetMaxPoint() const;

		// Get the number of values (i.e. coordinates on the X axis)
		int64_t GetLength() const;

		/// Get the number of points (i.e. # of points)
		int64_t GetCount() const;

		/// Get the direction of the curve at a specific index (increasing or decreasing)
		bool IsIncreasing(int index) const;

		// Get and Set JSON methods
		std::string Json() const; ///< Generate JSON string of this object
		Json::Value JsonValue() const; ///< Generate Json::Value for this object
		void SetJson(const std::string value); ///< Load JSON string into this object
		void SetJsonValue(const Json::Value root); ///< Load Json::Value into this object

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
		void PrintPoints(std::ostream* out=&std::cout) const;

		/// Print just the Y value of the point's primary coordinate
		void PrintValues(std::ostream* out=&std::cout) const;

	};

}

#endif
