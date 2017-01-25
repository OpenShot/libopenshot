/**
 * @file
 * @brief Source file for the Keyframe class
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

#include "../include/KeyFrame.h"

using namespace std;
using namespace openshot;

// Because points can be added in any order, we need to reorder them
// in ascending order based on the point.co.X value.  This simplifies
// processing the curve, due to all the points going from left to right.
void Keyframe::ReorderPoints() {
	// Loop through all coordinates, and sort them by the X attribute
	for (long int x = 0; x < Points.size(); x++) {
		long int compare_index = x;
		long int smallest_index = x;

		for (long int compare_index = x + 1; compare_index < Points.size(); compare_index++) {
			if (Points[compare_index].co.X < Points[smallest_index].co.X) {
				smallest_index = compare_index;
			}
		}

		// swap items
		if (smallest_index != compare_index) {
			swap(Points[compare_index], Points[smallest_index]);
		}
	}
}

// Constructor which sets the default point & coordinate at X=0
Keyframe::Keyframe(double value) : needs_update(true) {
	// Init the factorial table, needed by bezier curves
	CreateFactorialTable();

	// Add initial point
	AddPoint(Point(value));
}

// Keyframe constructor
Keyframe::Keyframe() : needs_update(true) {
	// Init the factorial table, needed by bezier curves
	CreateFactorialTable();
}

// Add a new point on the key-frame.  Each point has a primary coordinate,
// a left handle, and a right handle.
void Keyframe::AddPoint(Point p) {
	// mark as dirty
	needs_update = true;

	// Check for duplicate point (and remove it)
	Point closest = GetClosestPoint(p);
	if (closest.co.X == p.co.X)
		// Remove existing point
		RemovePoint(closest);

	// Add point at correct spot
	Points.push_back(p);

	// Sort / Re-order points based on X coordinate
	ReorderPoints();
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
long int Keyframe::FindIndex(Point p) throw(OutOfBoundsPoint) {
	// loop through points, and find a matching coordinate
	for (long int x = 0; x < Points.size(); x++) {
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
bool Keyframe::Contains(Point p) {
	// loop through points, and find a matching coordinate
	for (long int x = 0; x < Points.size(); x++) {
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
Point Keyframe::GetClosestPoint(Point p, bool useLeft) {
	Point closest(-1, -1);

	// loop through points, and find a matching coordinate
	for (long int x = 0; x < Points.size(); x++) {
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
Point Keyframe::GetClosestPoint(Point p) {
	return GetClosestPoint(p, false);
}

// Get previous point (if any)
Point Keyframe::GetPreviousPoint(Point p) {

	// Lookup the index of this point
	try {
		long int index = FindIndex(p);

		// If not the 1st point
		if (index > 0)
			return Points[index - 1];
		else
			return Points[0];

	} catch (OutOfBoundsPoint) {
		// No previous point
		return Point(-1, -1);
	}
}

// Get max point (by Y coordinate)
Point Keyframe::GetMaxPoint() {
	Point maxPoint(-1, -1);

	// loop through points, and find the largest Y value
	for (long int x = 0; x < Points.size(); x++) {
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
double Keyframe::GetValue(long int index)
{
	// Check if it needs to be processed
	if (needs_update)
		Process();

	// Is index a valid point?
	if (index >= 0 && index < Values.size())
		// Return value
		return Values[index].Y;
	else if (index < 0 && Values.size() > 0)
		// Return the minimum value
		return Values[0].Y;
	else if (index >= Values.size() && Values.size() > 0)
		// return the maximum value
		return Values[Values.size() - 1].Y;
	else
		// return a blank coordinate (0,0)
		return 0.0;
}

// Get the rounded INT value at a specific index
int Keyframe::GetInt(long int index)
{
	// Check if it needs to be processed
	if (needs_update)
		Process();

	// Is index a valid point?
	if (index >= 0 && index < Values.size())
		// Return value
		return int(round(Values[index].Y));
	else if (index < 0 && Values.size() > 0)
		// Return the minimum value
		return int(round(Values[0].Y));
	else if (index >= Values.size() && Values.size() > 0)
		// return the maximum value
		return int(round(Values[Values.size() - 1].Y));
	else
		// return a blank coordinate (0,0)
		return 0;
}

// Get the rounded INT value at a specific index
long int Keyframe::GetLong(long int index)
{
	// Check if it needs to be processed
	if (needs_update)
		Process();

	// Is index a valid point?
	if (index >= 0 && index < Values.size())
		// Return value
		return long(round(Values[index].Y));
	else if (index < 0 && Values.size() > 0)
		// Return the minimum value
		return long(round(Values[0].Y));
	else if (index >= Values.size() && Values.size() > 0)
		// return the maximum value
		return long(round(Values[Values.size() - 1].Y));
	else
		// return a blank coordinate (0,0)
		return 0;
}

// Get the direction of the curve at a specific index (increasing or decreasing)
bool Keyframe::IsIncreasing(int index)
{
	// Check if it needs to be processed
	if (needs_update)
		Process();

	// Is index a valid point?
	if (index >= 0 && index < Values.size())
		// Return value
		return long(round(Values[index].IsIncreasing()));
	else if (index < 0 && Values.size() > 0)
		// Return the minimum value
		return long(round(Values[0].IsIncreasing()));
	else if (index >= Values.size() && Values.size() > 0)
		// return the maximum value
		return long(round(Values[Values.size() - 1].IsIncreasing()));
	else
		// return the default direction of most curves (i.e. increasing is true)
		return true;
}

// Generate JSON string of this object
string Keyframe::Json() {

	// Return formatted string
	return JsonValue().toStyledString();
}

// Generate Json::JsonValue for this object
Json::Value Keyframe::JsonValue() {

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
void Keyframe::SetJson(string value) throw(InvalidJSON) {

	// Parse JSON string into JSON objects
	Json::Value root;
	Json::Reader reader;
	bool success = reader.parse( value, root );
	if (!success)
		// Raise exception
		throw InvalidJSON("JSON could not be parsed (or is invalid)", "");

	try
	{
		// Set all values that match
		SetJsonValue(root);
	}
	catch (exception e)
	{
		// Error parsing JSON (or missing keys)
		throw InvalidJSON("JSON is invalid (missing keys or invalid data types)", "");
	}
}

// Load Json::JsonValue into this object
void Keyframe::SetJsonValue(Json::Value root) {

	// mark as dirty
	needs_update = true;

	// Clear existing points
	Points.clear();

	if (!root["Points"].isNull())
		// loop through points
		for (long int x = 0; x < root["Points"].size(); x++) {
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
Fraction Keyframe::GetRepeatFraction(long int index)
{
	// Check if it needs to be processed
	if (needs_update)
		Process();

	// Is index a valid point?
	if (index >= 0 && index < Values.size())
		// Return value
		return Values[index].Repeat();
	else if (index < 0 && Values.size() > 0)
		// Return the minimum value
		return Values[0].Repeat();
	else if (index >= Values.size() && Values.size() > 0)
		// return the maximum value
		return Values[Values.size() - 1].Repeat();
	else
		// return a blank coordinate (0,0)
		return Fraction(1,1);
}

// Get the change in Y value (from the previous Y value)
double Keyframe::GetDelta(long int index)
{
	// Check if it needs to be processed
	if (needs_update)
		Process();

	// Is index a valid point?
	if (index >= 0 && index < Values.size())
		// Return value
		return Values[index].Delta();
	else if (index < 0 && Values.size() > 0)
		// Return the minimum value
		return Values[0].Delta();
	else if (index >= Values.size() && Values.size() > 0)
		// return the maximum value
		return Values[Values.size() - 1].Delta();
	else
		// return a blank coordinate (0,0)
		return 0.0;
}

// Get a point at a specific index
Point& Keyframe::GetPoint(long int index) throw(OutOfBoundsPoint) {
	// Is index a valid point?
	if (index >= 0 && index < Points.size())
		return Points[index];
	else
		// Invalid index
		throw OutOfBoundsPoint("Invalid point requested", index, Points.size());
}

// Get the number of values (i.e. coordinates on the X axis)
long int Keyframe::GetLength() {
	// Check if it needs to be processed
	if (needs_update)
		Process();

	// return the size of the Values vector
	return Values.size();
}

// Get the number of points (i.e. # of points)
long int Keyframe::GetCount() {

	// return the size of the Values vector
	return Points.size();
}

// Remove a point by matching a coordinate
void Keyframe::RemovePoint(Point p) throw(OutOfBoundsPoint) {
	// mark as dirty
	needs_update = true;

	// loop through points, and find a matching coordinate
	for (long int x = 0; x < Points.size(); x++) {
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
void Keyframe::RemovePoint(long int index) throw(OutOfBoundsPoint) {
	// mark as dirty
	needs_update = true;

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

void Keyframe::UpdatePoint(long int index, Point p) {
	// mark as dirty
	needs_update = true;

	// Remove matching point
	RemovePoint(index);

	// Add new point
	AddPoint(p);

	// Reorder points
	ReorderPoints();
}

void Keyframe::PrintPoints() {
	// Check if it needs to be processed
	if (needs_update)
		Process();

	cout << fixed << setprecision(4);
	for (vector<Point>::iterator it = Points.begin(); it != Points.end(); it++) {
		Point p = *it;
		cout << p.co.X << "\t" << p.co.Y << endl;
	}
}

void Keyframe::PrintValues() {
	// Check if it needs to be processed
	if (needs_update)
		Process();

	cout << fixed << setprecision(4);
	cout << "Frame Number (X)\tValue (Y)\tIs Increasing\tRepeat Numerator\tRepeat Denominator\tDelta (Y Difference)" << endl;

	for (vector<Coordinate>::iterator it = Values.begin() + 1; it != Values.end(); it++) {
		Coordinate c = *it;
		cout << long(round(c.X)) << "\t" << c.Y << "\t" << c.IsIncreasing() << "\t" << c.Repeat().num << "\t" << c.Repeat().den << "\t" << c.Delta() << endl;
	}
}

void Keyframe::Process() {

	#pragma omp critical (keyframe_process)
	{
		// only process if needed
		if (needs_update && Points.size() == 0) {
			// Clear all values
			Values.clear();
		}
		else if (needs_update && Points.size() > 0)
		{
			// Clear all values
			Values.clear();

			// fill in all values between 1 and 1st point's co.X
			Point p1 = Points[0];
			if (Points.size() > 1)
				// Fill in previous X values (before 1st point)
				for (long int x = 0; x < p1.co.X; x++)
					Values.push_back(Coordinate(Values.size(), p1.co.Y));
			else
				// Add a single value (since we only have 1 point)
				Values.push_back(Coordinate(Values.size(), p1.co.Y));

			// Loop through each pair of points (1 less than the max points).  Each
			// pair of points is used to process a segment of the keyframe.
			Point p2(0, 0);
			for (long int x = 0; x < Points.size() - 1; x++) {
				p1 = Points[x];
				p2 = Points[x + 1];

				// process segment p1,p2
				ProcessSegment(x, p1, p2);
			}

			// Loop through each Value, and set the direction of the coordinate.  This is used
			// when time mapping, to determine what direction the audio waveforms play.
			bool increasing = true;
			int repeat_count = 1;
			long int last_value = 0;
			for (vector<Coordinate>::iterator it = Values.begin() + 1; it != Values.end(); it++) {
				int current_value = long(round((*it).Y));
				long int next_value = long(round((*it).Y));
				long int prev_value = long(round((*it).Y));
				if (it + 1 != Values.end())
					next_value = long(round((*(it + 1)).Y));
				if (it - 1 >= Values.begin())
					prev_value = long(round((*(it - 1)).Y));

				// Loop forward and look for the next unique value (to determine direction)
				for (vector<Coordinate>::iterator direction_it = it + 1; direction_it != Values.end(); direction_it++) {
					long int next = long(round((*direction_it).Y));

					// Detect direction
					if (current_value < next)
					{
						increasing = true;
						break;
					}
					else if (current_value > next)
					{
						increasing = false;
						break;
					}
				}

				// Set direction
				(*it).IsIncreasing(increasing);

				// Detect repeated Y value
				if (current_value == last_value)
					// repeated, so increment count
					repeat_count++;
				else
					// reset repeat counter
					repeat_count = 1;

				// Detect how many 'more' times it's repeated
				int additional_repeats = 0;
				for (vector<Coordinate>::iterator repeat_it = it + 1; repeat_it != Values.end(); repeat_it++) {
					long int next = long(round((*repeat_it).Y));
					if (next == current_value)
						// repeated, so increment count
						additional_repeats++;
					else
						break; // stop looping
				}

				// Set repeat fraction
				(*it).Repeat(Fraction(repeat_count, repeat_count + additional_repeats));

				// Set delta (i.e. different from previous unique Y value)
				(*it).Delta(current_value - last_value);

				// track the last value
				last_value = current_value;
			}
		}

		// reset flag
		needs_update = false;
	}
}

void Keyframe::ProcessSegment(int Segment, Point p1, Point p2) {
	// Determine the number of values for this segment
	long int number_of_values = round(p2.co.X) - round(p1.co.X);

	// Exit function if no values
	if (number_of_values == 0)
		return;

	// Based on the interpolation mode, fill the "values" vector with the coordinates
	// for this segment
	switch (p2.interpolation) {

	// Calculate the "values" for this segment in with a LINEAR equation, effectively
	// creating a straight line with coordinates.
	case LINEAR: {
		// Get the difference in value
		double current_value = p1.co.Y;
		double value_difference = p2.co.Y - p1.co.Y;
		double value_increment = 0.0f;

		// Get the increment value, but take into account the
		// first segment has 1 extra value
		value_increment = value_difference / (double) (number_of_values);

		if (Segment == 0)
			// Add an extra value to the first segment
			number_of_values++;
		else
			// If not 1st segment, skip the first value
			current_value += value_increment;

		// Add each increment to the values vector
		for (long int x = 0; x < number_of_values; x++) {
			// add value as a coordinate to the "values" vector
			Values.push_back(Coordinate(Values.size(), current_value));

			// increment value
			current_value += value_increment;
		}

		break;
	}

		// Calculate the "values" for this segment using a quadratic Bezier curve.  This creates a
		// smooth curve.
	case BEZIER: {

		// Always increase the number of points by 1 (need all possible points
		// to correctly calculate the curve).
		number_of_values++;
		number_of_values *= 4;	// We need a higher resolution curve (4X)

		// Diff between points
		double X_diff = p2.co.X - p1.co.X;
		double Y_diff = p2.co.Y - p1.co.Y;

		vector<Coordinate> segment_coordinates;
		segment_coordinates.push_back(p1.co);
		segment_coordinates.push_back(Coordinate(p1.co.X + (p1.handle_right.X * X_diff), p1.co.Y + (p1.handle_right.Y * Y_diff)));
		segment_coordinates.push_back(Coordinate(p1.co.X + (p2.handle_left.X * X_diff), p1.co.Y + (p2.handle_left.Y * Y_diff)));
		segment_coordinates.push_back(p2.co);

		vector<Coordinate> raw_coordinates;
		long int npts = segment_coordinates.size();
		long int icount, jcount;
		double step, t;
		double last_x = -1; // small number init, to track the last used x

		// Calculate points on curve
		icount = 0;
		t = 0;

		step = (double) 1.0 / (number_of_values - 1);

		for (long int i1 = 0; i1 < number_of_values; i1++) {
			if ((1.0 - t) < 5e-6)
				t = 1.0;

			jcount = 0;

			double new_x = 0.0f;
			double new_y = 0.0f;

			for (long int i = 0; i < npts; i++) {
				Coordinate co = segment_coordinates[i];
				double basis = Bernstein(npts - 1, i, t);
				new_x += basis * co.X;
				new_y += basis * co.Y;
			}

			// Add new value to the vector
			Coordinate current_value(new_x, new_y);

			// Add all values for 1st segment
			raw_coordinates.push_back(current_value);

			// increment counters
			icount += 2;
			t += step;
		}

		// Loop through the raw coordinates, and map them correctly to frame numbers.  For example,
		// we can't have duplicate X values, since X represents our frame  numbers.
		long int current_frame = p1.co.X;
		double current_value = p1.co.Y;
		for (long int i = 0; i < raw_coordinates.size(); i++)
		{
			// Get the raw coordinate
			Coordinate raw = raw_coordinates[i];

			if (current_frame == round(raw.X))
				// get value of raw coordinate
				current_value = raw.Y;
			else
			{
				// Missing X values (use last known Y values)
				long int number_of_missing = round(raw.X) - current_frame;
				for (long int missing = 0; missing < number_of_missing; missing++)
				{
					// Add new value to the vector
					Coordinate new_coord(current_frame, current_value);

					if (Segment == 0 || Segment > 0 && current_frame > p1.co.X)
						// Add to "values" vector
						Values.push_back(new_coord);

					// Increment frame
					current_frame++;
				}

				// increment the current value
				current_value = raw.Y;
			}
		}

		// Add final coordinate
		Coordinate new_coord(current_frame, current_value);
		Values.push_back(new_coord);

		break;
	}

		// Calculate the "values" of this segment by maintaining the value of p1 until the
		// last point, and then make the value jump to p2.  This effectively just jumps
		// the value, instead of ramping up or down the value.
	case CONSTANT: {

		if (Segment == 0)
			// first segment has 1 extra value
			number_of_values++;

		// Add each increment to the values vector
		for (long int x = 0; x < number_of_values; x++) {
			if (x < (number_of_values - 1)) {
				// Not the last value of this segment
				// add coordinate to "values"
				Values.push_back(Coordinate(Values.size(), p1.co.Y));
			} else {
				// This is the last value of this segment
				// add coordinate to "values"
				Values.push_back(Coordinate(Values.size(), p2.co.Y));
			}
		}
		break;
	}

	}
}

// Create lookup table for fast factorial calculation
void Keyframe::CreateFactorialTable() {
	// Only 4 lookups are needed, because we only support 4 coordinates per curve
	FactorialLookup[0] = 1.0;
	FactorialLookup[1] = 1.0;
	FactorialLookup[2] = 2.0;
	FactorialLookup[3] = 6.0;
}

// Get a factorial for a coordinate
double Keyframe::Factorial(long int n) {
	assert(n >= 0 && n <= 3);
	return FactorialLookup[n]; /* returns the value n! as a SUMORealing point number */
}

// Calculate the factorial function for Bernstein basis
double Keyframe::Ni(long int n, long int i) {
	double ni;
	double a1 = Factorial(n);
	double a2 = Factorial(i);
	double a3 = Factorial(n - i);
	ni = a1 / (a2 * a3);
	return ni;
}

// Calculate Bernstein basis
double Keyframe::Bernstein(long int n, long int i, double t) {
	double basis;
	double ti; /* t^i */
	double tni; /* (1 - t)^i */

	/* Prevent problems with pow */
	if (t == 0.0 && i == 0)
		ti = 1.0;
	else
		ti = pow(t, i);

	if (n == i && t == 1.0)
		tni = 1.0;
	else
		tni = pow((1 - t), (n - i));

	// Bernstein basis
	basis = Ni(n, i) * ti * tni;
	return basis;
}

// Scale all points by a percentage (good for evenly lengthening or shortening an openshot::Keyframe)
// 1.0 = same size, 1.05 = 5% increase, etc...
void Keyframe::ScalePoints(double scale)
{
	// Loop through each point (skipping the 1st point)
	for (long int point_index = 0; point_index < Points.size(); point_index++) {
		// Skip the 1st point
		if (point_index == 0)
			continue;

		// Scale X value
		Points[point_index].co.X = round(Points[point_index].co.X * scale);

		// Mark for re-processing
		needs_update = true;
	}
}

// Flip all the points in this openshot::Keyframe (useful for reversing an effect or transition, etc...)
void Keyframe::FlipPoints()
{
	// Loop through each point
	vector<Point> FlippedPoints;
	for (long int point_index = 0, reverse_index = Points.size() - 1; point_index < Points.size(); point_index++, reverse_index--) {
		// Flip the points
		Point p = Points[point_index];
		p.co.Y = Points[reverse_index].co.Y;
		FlippedPoints.push_back(p);
	}

	// Swap vectors
	Points.swap(FlippedPoints);

	// Mark for re-processing
	needs_update = true;
}
