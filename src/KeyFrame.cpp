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

#include "../include/KeyFrame.h"
#include <algorithm>
#include <utility>

using namespace std;
using namespace openshot;

namespace {
	bool IsPointBeforeX(Point const & p, double const x) {
		return p.co.X < x;
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
	for (int64_t x = 0; x < Points.size(); x++) {
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
	// loop through points, and find a matching coordinate
	for (int64_t x = 0; x < Points.size(); x++) {
		// Get each point
		Point existing_point = Points[x];

		// find a match
		if (p.co.X == existing_point.co.X) {
			// Remove the matching point, and break out of loop
			return true;
		}
	}

	// no matching point found
	return false;
}

// Get current point (or closest point) from the X coordinate (i.e. the frame number)
Point Keyframe::GetClosestPoint(Point p, bool useLeft) const {
	Point closest(-1, -1);

	// loop through points, and find a matching coordinate
	for (int64_t x = 0; x < Points.size(); x++) {
		// Get each point
		Point existing_point = Points[x];

		// find a match
		if (existing_point.co.X >= p.co.X && !useLeft) {
			// New closest point found (to the Right)
			closest = existing_point;
			break;
		} else if (existing_point.co.X < p.co.X && useLeft) {
			// New closest point found (to the Left)
			closest = existing_point;
		} else if (existing_point.co.X >= p.co.X && useLeft) {
			// We've gone past the left point... so break
			break;
		}
	}

	// Handle edge cases (if no point was found)
	if (closest.co.X == -1) {
		if (p.co.X <= 1 && Points.size() > 0)
			// Assign 1st point
			closest = Points[0];
		else if (Points.size() > 0)
			// Assign last point
			closest = Points[Points.size() - 1];
	}

	// no matching point found
	return closest;
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

	// loop through points, and find the largest Y value
	for (int64_t x = 0; x < Points.size(); x++) {
		// Get each point
		Point existing_point = Points[x];

		// Is point larger than max point
		if (existing_point.co.Y >= maxPoint.co.Y) {
			// New max point found
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
	assert(predecessor->co.X < index);
	assert(index < candidate->co.X);

	// CONSTANT and LINEAR interpolations are fast to compute!
	switch (candidate->interpolation) {
	case CONSTANT: return predecessor->co.Y;
	case LINEAR: {
		double const diff_Y = candidate->co.Y - predecessor->co.Y;
		double const diff_X = candidate->co.X - predecessor->co.X;
		double const slope = diff_Y / diff_X;
		return predecessor->co.Y + slope * (index - predecessor->co.X);
	}
	case BEZIER: break;
	}

	// BEZIER curve!
	assert(candidate->interpolation == BEZIER);

	double const X_diff = candidate->co.X - predecessor->co.X;
	double const Y_diff = candidate->co.Y - predecessor->co.Y;
	Coordinate const p0 = predecessor->co;
	Coordinate const p1 = Coordinate(p0.X + predecessor->handle_right.X * X_diff, p0.Y + predecessor->handle_right.Y * Y_diff);
	Coordinate const p2 = Coordinate(p0.X + candidate->handle_left.X * X_diff, p0.Y + candidate->handle_left.Y * Y_diff);
	Coordinate const p3 = candidate->co;

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
		if (abs(index - x) < 0.01) {
			return y;
		}
		if (x > index) {
			t -= t_step;
		}
		else {
			t += t_step;
		}
		t_step /= 2;
	} while (true);
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
	int64_t const current = GetLong(index);
	// TODO: skip over constant sections.
	do {
		int64_t const next = GetLong(++index);
		if (next > current) return true;
		if (next < current) return false;
	} while (index < GetLength());
	return false;
}

// Generate JSON string of this object
std::string Keyframe::Json() const {

	// Return formatted string
	return JsonValue().toStyledString();
}

// Generate Json::JsonValue for this object
Json::Value Keyframe::JsonValue() const {

	// Create root json object
	Json::Value root;
	root["Points"] = Json::Value(Json::arrayValue);

	// loop through points, and find a matching coordinate
	for (int x = 0; x < Points.size(); x++) {
		// Get each point
		Point existing_point = Points[x];
		root["Points"].append(existing_point.JsonValue());
	}

	// return JsonValue
	return root;
}

// Load JSON string into this object
void Keyframe::SetJson(std::string value) {

	// Parse JSON string into JSON objects
	Json::Value root;
	Json::CharReaderBuilder rbuilder;
	Json::CharReader* reader(rbuilder.newCharReader());

	std::string errors;
	bool success = reader->parse( value.c_str(),
                 value.c_str() + value.size(), &root, &errors );
	delete reader;

	if (!success)
		// Raise exception
		throw InvalidJSON("JSON could not be parsed (or is invalid)");

	try
	{
		// Set all values that match
		SetJsonValue(root);
	}
	catch (const std::exception& e)
	{
		// Error parsing JSON (or missing keys)
		throw InvalidJSON("JSON is invalid (missing keys or invalid data types)");
	}
}

// Load Json::JsonValue into this object
void Keyframe::SetJsonValue(Json::Value root) {
	// Clear existing points
	Points.clear();

	if (!root["Points"].isNull())
		// loop through points
		for (int64_t x = 0; x < root["Points"].size(); x++) {
			// Get each point
			Json::Value existing_point = root["Points"][(Json::UInt) x];

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
	// Is index a valid point?
	if (index >= 1 && (index + 1) < GetLength()) {
		int64_t current_value = GetLong(index);
		int64_t previous_repeats = 0;
		int64_t next_repeats = 0;

		// Loop backwards and look for the next unique value
		for (int64_t i = index; i > 0; --i) {
			if (GetLong(i) == current_value) {
				// Found same value
				previous_repeats++;
			} else {
				// Found non repeating value, no more repeats found
				break;
			}
		}

		// Loop forwards and look for the next unique value
		for (int64_t i = index + 1; i < GetLength(); ++i) {
			if (GetLong(i) == current_value) {
				// Found same value
				next_repeats++;
			} else {
				// Found non repeating value, no more repeats found
				break;
			}
		}

		int64_t total_repeats = previous_repeats + next_repeats;
		return Fraction(previous_repeats, total_repeats);
	}
	else
		// return a blank coordinate
		return Fraction(1,1);
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
	if (index >= 0 && index < Points.size())
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
	for (int64_t x = 0; x < Points.size(); x++) {
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
	if (index >= 0 && index < Points.size())
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

	for (uint64_t i = 1; i < GetLength(); ++i) {
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
	for (int64_t point_index = 1; point_index < Points.size(); point_index++) {
		// Scale X value
		Points[point_index].co.X = round(Points[point_index].co.X * scale);
	}
}

// Flip all the points in this openshot::Keyframe (useful for reversing an effect or transition, etc...)
void Keyframe::FlipPoints() {
	for (int64_t point_index = 0, reverse_index = Points.size() - 1; point_index < reverse_index; point_index++, reverse_index--) {
		// Flip the points
		using std::swap;
		swap(Points[point_index].co.Y, Points[reverse_index].co.Y);
		// TODO: check that this has the desired effect even with
		// regards to handles!
	}
}
