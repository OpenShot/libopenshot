/**
 * @file
 * @brief Source file for the Keyframe class
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

#include "KeyFrame.h"
#include <algorithm>
#include <functional>
#include <utility>

using namespace std;
using namespace openshot;

namespace {
	bool IsPointBeforeX(Point const & p, double const x) {
		return p.co.X < x;
	}

	double InterpolateLinearCurve(Point const & left, Point const & right, double const target) {
		double const diff_Y = right.co.Y - left.co.Y;
		double const diff_X = right.co.X - left.co.X;
		double const slope = diff_Y / diff_X;
		return left.co.Y + slope * (target - left.co.X);
	}

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


	double InterpolateBetween(Point const & left, Point const & right, double target, double allowed_error) {
		assert(left.co.X < target);
		assert(target <= right.co.X);
		switch (right.interpolation) {
		case CONSTANT: return left.co.Y;
		case LINEAR: return InterpolateLinearCurve(left, right, target);
		case BEZIER: return InterpolateBezierCurve(left, right, target, allowed_error);
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
}


// Constructor which sets the default point & coordinate at X=1
Keyframe::Keyframe(double value) {
	// Add initial point
	AddPoint(Point(value));
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

// Add a new point on the key-frame, with some defaults set (BEZIER)
void Keyframe::AddPoint(double x, double y)
{
	// Create a point
	Point new_point(x, y, BEZIER);

	// Add the point
	AddPoint(new_point);
}

// Add a new point on the key-frame, with a specific interpolation type
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
	if (index < 1 || (index + 1) >= GetLength()) {
		return true;
	}
	std::vector<Point>::const_iterator candidate =
		std::lower_bound(begin(Points), end(Points), static_cast<double>(index), IsPointBeforeX);
	if (candidate == end(Points)) {
		return false; // After the last point, thus constant.
	}
	if ((candidate->co.X == index) || (candidate == begin(Points))) {
		++candidate;
	}
	int64_t const value = GetLong(index);
	do {
		if (value < round(candidate->co.Y)) {
			return true;
		} else if (value > round(candidate->co.Y)) {
			return false;
		}
		++candidate;
	} while (candidate != end(Points));
	return false;
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

	if (!root["Points"].isNull())
		// loop through points
		for (const auto existing_point : root["Points"]) {
			// Create Point
			Point p;

			// Load Json into Point
			p.SetJsonValue(existing_point);

			// Add Point to Keyframe
			AddPoint(p);
		}
}

// Get the fraction that represents how many times this value is repeated in the curve
// This is depreciated and will be removed soon.
Fraction Keyframe::GetRepeatFraction(int64_t index) const {
	// Frame numbers (index) outside of the "defined" range of this
	// keyframe result in a 1/1 default value.
	if (index < 1 || (index + 1) >= GetLength()) {
		return Fraction(1,1);
	}
	assert(Points.size() > 1); // Due to ! ((index + 1) >= GetLength) there are at least two points!

	// First, get the value at the given frame and the closest point
	// to the right.
	int64_t const current_value = GetLong(index);
	std::vector<Point>::const_iterator const candidate =
		std::lower_bound(begin(Points), end(Points), static_cast<double>(index), IsPointBeforeX);
	assert(candidate != end(Points)); // Due to the (index + 1) >= GetLength check above!

	// Calculate how many of the next values are going to be the same:
	int64_t next_repeats = 0;
	std::vector<Point>::const_iterator i = candidate;
	// If the index (frame number) is the X coordinate of the closest
	// point, then look at the segment to the right; the "current"
	// segement is not interesting because we're already at the last
	// value of it.
	if (i->co.X == index) {
		++i;
	}
	// Skip over "constant" (when rounded) segments.
	bool all_constant = true;
	for (; i != end(Points); ++i) {
		if (current_value != round(i->co.Y)) {
			all_constant = false;
			break;
		}
	}
	if (! all_constant) {
		// Found a point which defines a segment which will give a
		// different value than the current value.  This means we
		// moved at least one segment to the right, thus we cannot be
		// at the first point.
		assert(i != begin(Points));
		Point const left = *(i - 1);
		Point const right = *i;
		int64_t change_at;
		if (current_value < round(i->co.Y)) {
			change_at = SearchBetweenPoints(left, right, current_value, std::less_equal<double>{});
		} else {
			assert(current_value > round(i->co.Y));
			change_at = SearchBetweenPoints(left, right, current_value, std::greater_equal<double>{});
		}
		next_repeats = change_at - index;
	} else {
		// All values to the right are the same!
		next_repeats = Points.back().co.X - index;
	}

	// Now look to the left, to the previous values.
	all_constant = true;
	i = candidate;
	if (i != begin(Points)) {
		// The binary search below assumes i to be the left point;
		// candidate is the right point of the current segment
		// though. So change this if possible. If this branch is NOT
		// taken, then we're at/before the first point and all is
		// constant!
		--i;
	}
	int64_t previous_repeats = 0;
	// Skip over constant (when rounded) segments!
	for (; i != begin(Points); --i) {
		if (current_value != round(i->co.Y)) {
			all_constant = false;
			break;
		}
	}
	// Special case when skipped until the first point, but the first
	// point is actually different.  Will not happen if index is
	// before the first point!
	if (current_value != round(i->co.Y)) {
		assert(i != candidate);
		all_constant = false;
	}
	if (! all_constant) {
		// There are at least two points, and we're not at the end,
		// thus the following is safe!
		Point const left = *i;
		Point const right = *(i + 1);
		int64_t change_at;
		if (current_value > round(left.co.Y)) {
			change_at = SearchBetweenPoints(left, right, current_value, std::less<double>{});
		} else {
			assert(current_value < round(left.co.Y));
			change_at = SearchBetweenPoints(left, right, current_value, std::greater<double>{});
		}
		previous_repeats = index - change_at;
	} else {
		// Every previous value is the same (rounded) as the current
		// value.
		previous_repeats = index;
	}
	int64_t total_repeats = previous_repeats + next_repeats;
	return Fraction(previous_repeats, total_repeats);
}

// Get the change in Y value (from the previous Y value)
double Keyframe::GetDelta(int64_t index) const {
	if (index < 1) return 0;
	if (index == 1 && ! Points.empty()) return Points[0].co.Y;
	if (index >= GetLength()) return 0;
	return GetLong(index) - GetLong(index - 1);
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
	return round(Points.back().co.X) + 1;
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

void Keyframe::UpdatePoint(int64_t index, Point p) {
	// Remove matching point
	RemovePoint(index);

	// Add new point
	AddPoint(p);
}

void Keyframe::PrintPoints() const {
	cout << fixed << setprecision(4);
	for (std::vector<Point>::const_iterator it = Points.begin(); it != Points.end(); it++) {
		Point p = *it;
		cout << p.co.X << "\t" << p.co.Y << endl;
	}
}

void Keyframe::PrintValues() const {
	cout << fixed << setprecision(4);
	cout << "Frame Number (X)\tValue (Y)\tIs Increasing\tRepeat Numerator\tRepeat Denominator\tDelta (Y Difference)\n";

	for (int64_t i = 1; i < GetLength(); ++i) {
		cout << i << "\t" << GetValue(i) << "\t" << IsIncreasing(i) << "\t" ;
		cout << GetRepeatFraction(i).num << "\t" << GetRepeatFraction(i).den << "\t" << GetDelta(i) << "\n";
	}
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
