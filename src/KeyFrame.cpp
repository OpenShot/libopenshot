/**
 * @file
 * @brief Source file for the Keyframe class
 * @author Jonathan Thomas <jonathan@openshot.org>
 *
 * @ref License
 */

// Copyright (c) 2008-2019 OpenShot Studios, LLC
//
// SPDX-License-Identifier: LGPL-3.0-or-later

#include "KeyFrame.h"
#include "Exceptions.h"

#include <algorithm>   // For std::lower_bound, std::move_backward
#include <functional>  // For std::less, std::less_equal, etc…
#include <utility>	 // For std::swap
#include <numeric>	 // For std::accumulate
#include <cassert>	 // For assert()
#include <cmath>	   // For fabs, round
#include <iostream>	// For std::cout
#include <iomanip>	 // For std::setprecision

using namespace std;
using namespace openshot;

namespace openshot{

	// Check if the X coordinate of a given Point is lower than a given value
	bool IsPointBeforeX(Point const & p, double const x) {
		return p.co.X < x;
	}

	// Linear interpolation between two points
	double InterpolateLinearCurve(Point const & left, Point const & right, double const target) {
		double const diff_Y = right.co.Y - left.co.Y;
		double const diff_X = right.co.X - left.co.X;
		double const slope = diff_Y / diff_X;
		return left.co.Y + slope * (target - left.co.X);
	}

	// Bezier interpolation between two points
	double InterpolateBezierCurve(Point const & left, Point const & right, double const target, double const allowed_error) {
		double const X_diff = right.co.X - left.co.X;
		double const Y_diff = right.co.Y - left.co.Y;
		Coordinate const p0 = left.co;
		Coordinate const p1 = Coordinate(p0.X + left.handle_right.X * X_diff, p0.Y + left.handle_right.Y * Y_diff);
		Coordinate const p2 = Coordinate(p0.X + right.handle_left.X * X_diff, p0.Y + right.handle_left.Y * Y_diff);
		Coordinate const p3 = right.co;

		double t = 0.5;
		double t_step = 0.25;
		do {
			// Bernstein polynoms
			double B[4] = {1, 3, 3, 1};
			double oneMinTExp = 1;
			double tExp = 1;
			for (int i = 0; i < 4; ++i, tExp *= t) {
				B[i] *= tExp;
			}
			for (int i = 0; i < 4; ++i, oneMinTExp *= 1 - t) {
				B[4 - i - 1] *= oneMinTExp;
			}
			double const x = p0.X * B[0] + p1.X * B[1] + p2.X * B[2] + p3.X * B[3];
			double const y = p0.Y * B[0] + p1.Y * B[1] + p2.Y * B[2] + p3.Y * B[3];
			if (fabs(target - x) < allowed_error) {
				return y;
			}
			if (x > target) {
				t -= t_step;
			}
			else {
				t += t_step;
			}
			t_step /= 2;
		} while (true);
	}
	// Interpolate two points using the right Point's interpolation method
	double InterpolateBetween(Point const & left, Point const & right, double target, double allowed_error) {
		// check if target is outside of the extremities poits
		// This can occur when moving fast the play head
		if(left.co.X > target){
			return left.co.Y;
		}
		if(target > right.co.X){
			return right.co.Y;
		}
		switch (right.interpolation) {
		case CONSTANT: return left.co.Y;
		case LINEAR: return InterpolateLinearCurve(left, right, target);
		case BEZIER: return InterpolateBezierCurve(left, right, target, allowed_error);
		default: return InterpolateLinearCurve(left, right, target);
		}
	}
}

template<typename Check>
int64_t SearchBetweenPoints(Point const & left, Point const & right, int64_t const current, Check check) {
	int64_t start = left.co.X;
	int64_t stop = right.co.X;
	while (start < stop) {
		int64_t const mid = (start + stop + 1) / 2;
		double const value = InterpolateBetween(left, right, mid, 0.01);
		if (check(round(value), current)) {
			start = mid;
		} else {
			stop = mid - 1;
		}
	}
	return start;
}

// Constructor which sets the default point & coordinate at X=1
Keyframe::Keyframe(double value) {
	// Add initial point
	AddPoint(Point(1, value));
}

// Constructor which takes a vector of Points
Keyframe::Keyframe(const std::vector<openshot::Point>& points) : Points(points) {};

// Destructor
Keyframe::~Keyframe() {
	Points.clear();
	Points.shrink_to_fit();
}

// Add a new point on the key-frame.  Each point has a primary coordinate,
// a left handle, and a right handle.
void Keyframe::AddPoint(Point p) {
	// candidate is not less (greater or equal) than the new point in
	// the X coordinate.
	std::vector<Point>::iterator candidate =
		std::lower_bound(begin(Points), end(Points), p.co.X, IsPointBeforeX);
	if (candidate == end(Points)) {
		// New point X is greater than all other points' X, add to
		// back.
		Points.push_back(p);
	} else if ((*candidate).co.X == p.co.X) {
		// New point is at same X coordinate as some point, overwrite
		// point.
		*candidate = p;
	} else {
		// New point needs to be inserted before candidate; thus move
		// candidate and all following one to the right and insert new
		// point then where candidate was.
		size_t const candidate_index = candidate - begin(Points);
		Points.push_back(p); // Make space; could also be a dummy point. INVALIDATES candidate!
		std::move_backward(begin(Points) + candidate_index, end(Points) - 1, end(Points));
		Points[candidate_index] = p;
	}
}

// Add a new point on the key-frame, interpolate is optional (default: BEZIER)
void Keyframe::AddPoint(double x, double y, InterpolationType interpolate)
{
	// Create a point
	Point new_point(x, y, interpolate);

	// Add the point
	AddPoint(new_point);
}

// Get the index of a point by matching a coordinate
int64_t Keyframe::FindIndex(Point p) const {
	// loop through points, and find a matching coordinate
	for (std::vector<Point>::size_type x = 0; x < Points.size(); x++) {
		// Get each point
		Point existing_point = Points[x];

		// find a match
		if (p.co.X == existing_point.co.X && p.co.Y == existing_point.co.Y) {
			// Remove the matching point, and break out of loop
			return x;
		}
	}

	// no matching point found
	throw OutOfBoundsPoint("Invalid point requested", -1, Points.size());
}

// Determine if point already exists
bool Keyframe::Contains(Point p) const {
	std::vector<Point>::const_iterator i =
		std::lower_bound(begin(Points), end(Points), p.co.X, IsPointBeforeX);
	return i != end(Points) && i->co.X == p.co.X;
}

// Get current point (or closest point) from the X coordinate (i.e. the frame number)
Point Keyframe::GetClosestPoint(Point p, bool useLeft) const {
	if (Points.size() == 0) {
		return Point(-1, -1);
	}

	// Finds a point with an X coordinate which is "not less" (greater
	// or equal) than the queried X coordinate.
	std::vector<Point>::const_iterator candidate =
		std::lower_bound(begin(Points), end(Points), p.co.X, IsPointBeforeX);

	if (candidate == end(Points)) {
		// All points are before the queried point.
		//
		// Note: Behavior the same regardless of useLeft!
		return Points.back();
	}
	if (candidate == begin(Points)) {
		// First point is greater or equal to the queried point.
		//
		// Note: Behavior the same regardless of useLeft!
		return Points.front();
	}
	if (useLeft) {
		return *(candidate - 1);
	} else {
		return *candidate;
	}
}

// Get current point (or closest point to the right) from the X coordinate (i.e. the frame number)
Point Keyframe::GetClosestPoint(Point p) const {
	return GetClosestPoint(p, false);
}

// Get previous point (if any)
Point Keyframe::GetPreviousPoint(Point p) const {

	// Lookup the index of this point
	try {
		int64_t index = FindIndex(p);

		// If not the 1st point
		if (index > 0)
			return Points[index - 1];
		else
			return Points[0];

	} catch (const OutOfBoundsPoint& e) {
		// No previous point
		return Point(-1, -1);
	}
}

// Get max point (by Y coordinate)
Point Keyframe::GetMaxPoint() const {
	Point maxPoint(-1, -1);

	for (Point const & existing_point: Points) {
		if (existing_point.co.Y >= maxPoint.co.Y) {
			maxPoint = existing_point;
		}
	}

	return maxPoint;
}

// Get the value at a specific index
double Keyframe::GetValue(int64_t index) const {
	if (Points.empty()) {
		return 0;
	}
	std::vector<Point>::const_iterator candidate =
		std::lower_bound(begin(Points), end(Points), static_cast<double>(index), IsPointBeforeX);

	if (candidate == end(Points)) {
		// index is behind last point
		return Points.back().co.Y;
	}
	if (candidate == begin(Points)) {
		// index is at or before first point
		return Points.front().co.Y;
	}
	if (candidate->co.X == index) {
		// index is directly on a point
		return candidate->co.Y;
	}
	std::vector<Point>::const_iterator predecessor = candidate - 1;
	return InterpolateBetween(*predecessor, *candidate, index, 0.01);
}

// Get the rounded INT value at a specific index
int Keyframe::GetInt(int64_t index) const {
	return int(round(GetValue(index)));
}

// Get the rounded INT value at a specific index
int64_t Keyframe::GetLong(int64_t index) const {
	return long(round(GetValue(index)));
}

// Get the direction of the curve at a specific index (increasing or decreasing)
bool Keyframe::IsIncreasing(int index) const
{
	if (index <= 1) {
		// Determine direction of frame 1 (and assume previous frames have same direction)
		index = 1;
	} else  if (index >= GetLength()) {
		// Determine direction of last valid frame # (and assume next frames have same direction)
		index = GetLength() - 1;
	}

	// Get current index value
	const double current_value = GetValue(index);

	// Iterate from current index to next significant value change
	int attempts = 1;
	while (attempts < 600 && index + attempts <= GetLength()) {
		// Get next value
		const double next_value = GetValue(index + attempts);

		// Is value significantly different
		const double diff = next_value - current_value;
		if (fabs(diff) > 0.0001) {
			if (diff < 0.0) {
				// Decreasing value found next
				return false;
			} else {
				// Increasing value found next
				return true;
			}
		}

		// increment attempt
		attempts++;
	}

	// If no next value found, assume increasing values
	return true;
}

// Generate JSON string of this object
std::string Keyframe::Json() const {

	// Return formatted string
	return JsonValue().toStyledString();
}

// Generate Json::Value for this object
Json::Value Keyframe::JsonValue() const {

	// Create root json object
	Json::Value root;
	root["Points"] = Json::Value(Json::arrayValue);

	// loop through points
	for (const auto existing_point : Points) {
		root["Points"].append(existing_point.JsonValue());
	}

	// return JsonValue
	return root;
}

// Load JSON string into this object
void Keyframe::SetJson(const std::string value) {

	// Parse JSON string into JSON objects
	try
	{
		const Json::Value root = openshot::stringToJson(value);
		// Set all values that match
		SetJsonValue(root);
	}
	catch (const std::exception& e)
	{
		// Error parsing JSON (or missing keys)
		throw InvalidJSON("JSON is invalid (missing keys or invalid data types)");
	}
}

// Load Json::Value into this object
void Keyframe::SetJsonValue(const Json::Value root) {
	// Clear existing points
	Points.clear();
	Points.shrink_to_fit();

	if (root.isObject() && !root["Points"].isNull()) {
        // loop through points in JSON Object
        for (const auto existing_point : root["Points"]) {
            // Create Point
            Point p;

            // Load Json into Point
            p.SetJsonValue(existing_point);

            // Add Point to Keyframe
            AddPoint(p);
        }
    } else if (root.isNumeric()) {
        // Create Point from Numeric value
        Point p(root.asFloat());

        // Add Point to Keyframe
        AddPoint(p);
	}
}

// Get the change in Y value (from the previous Y value)
double Keyframe::GetDelta(int64_t index) const {
	if (index < 1) return 0.0;
	if (index == 1 && !Points.empty()) return Points[0].co.Y;
	if (index >= GetLength()) return 0.0;
	return GetValue(index) - GetValue(index - 1);
}

// Get a point at a specific index
Point const & Keyframe::GetPoint(int64_t index) const {
	// Is index a valid point?
	if (index >= 0 && index < (int64_t)Points.size())
		return Points[index];
	else
		// Invalid index
		throw OutOfBoundsPoint("Invalid point requested", index, Points.size());
}

// Get the number of values (i.e. coordinates on the X axis)
int64_t Keyframe::GetLength() const {
	if (Points.empty()) return 0;
	if (Points.size() == 1) return 1;
	return round(Points.back().co.X);
}

// Get the number of points (i.e. # of points)
int64_t Keyframe::GetCount() const {

	return Points.size();
}

// Remove a point by matching a coordinate
void Keyframe::RemovePoint(Point p) {
	// loop through points, and find a matching coordinate
	for (std::vector<Point>::size_type x = 0; x < Points.size(); x++) {
		// Get each point
		Point existing_point = Points[x];

		// find a match
		if (p.co.X == existing_point.co.X && p.co.Y == existing_point.co.Y) {
			// Remove the matching point, and break out of loop
			Points.erase(Points.begin() + x);
			return;
		}
	}

	// no matching point found
	throw OutOfBoundsPoint("Invalid point requested", -1, Points.size());
}

// Remove a point by index
void Keyframe::RemovePoint(int64_t index) {
	// Is index a valid point?
	if (index >= 0 && index < (int64_t)Points.size())
	{
		// Remove a specific point by index
		Points.erase(Points.begin() + index);
	}
	else
		// Invalid index
		throw OutOfBoundsPoint("Invalid point requested", index, Points.size());
}

// Replace an existing point with a new point
void Keyframe::UpdatePoint(int64_t index, Point p) {
	// Remove matching point
	RemovePoint(index);

	// Add new point
	AddPoint(p);
}

void Keyframe::PrintPoints(std::ostream* out) const {
	*out << std::right << std::setprecision(4) << std::setfill(' ');
	for (const auto& p : Points) {
		*out << std::defaultfloat
			 << std::setw(6) << p.co.X
			 << std::setw(14) << std::fixed << p.co.Y
			 << '\n';
	}
	*out << std::flush;
}

void Keyframe::PrintValues(std::ostream* out) const {
	// Column widths
	std::vector<int> w{10, 12, 8, 11, 19};

	*out << std::right << std::setfill(' ') << std::setprecision(4);
	// Headings
	*out << "│"
		 << std::setw(w[0]) << "Frame# (X)" << " │"
		 << std::setw(w[1]) << "Y Value" << " │"
		 << std::setw(w[2]) << "Delta Y" << " │ "
		 << std::setw(w[3]) << "Increasing?" << std::right
		 << "│\n";
	// Divider
	*out << "├───────────"
		 << "┼─────────────"
		 << "┼─────────"
		 << "┼────────────┤\n";

	for (int64_t i = 1; i <= GetLength(); ++i) {
		*out << "│"
			 << std::setw(w[0]-2) << std::defaultfloat << i
			 << (Contains(Point(i, 1)) ? " *" : "  ") << " │"
			 << std::setw(w[1]) << std::fixed << GetValue(i) << " │"
			 << std::setw(w[2]) << std::defaultfloat << std::showpos
								<< GetDelta(i) << " │ " << std::noshowpos
			 << std::setw(w[3])
			 << (IsIncreasing(i) ? "true" : "false") << std::right << "│\n";
	}
	*out << " * = Keyframe point (non-interpolated)\n";
	*out << std::flush;
}


// Scale all points by a percentage (good for evenly lengthening or shortening an openshot::Keyframe)
// 1.0 = same size, 1.05 = 5% increase, etc...
void Keyframe::ScalePoints(double scale)
{
	// TODO: What if scale is small so that two points land on the
	// same X coordinate?
	// TODO: What if scale < 0?

	// Loop through each point (skipping the 1st point)
	for (std::vector<Point>::size_type point_index = 1; point_index < Points.size(); point_index++) {
		// Scale X value
		Points[point_index].co.X = round(Points[point_index].co.X * scale);
	}
}

// Flip all the points in this openshot::Keyframe (useful for reversing an effect or transition, etc...)
void Keyframe::FlipPoints() {
	for (std::vector<Point>::size_type point_index = 0, reverse_index = Points.size() - 1; point_index < reverse_index; point_index++, reverse_index--) {
		// Flip the points
		using std::swap;
		swap(Points[point_index].co.Y, Points[reverse_index].co.Y);
		// TODO: check that this has the desired effect even with
		// regards to handles!
	}
}
