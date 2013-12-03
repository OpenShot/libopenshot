/**
 * @file
 * @brief Source file for the Keyframe class
 * @author Jonathan Thomas <jonathan@openshot.org>
 *
 * @section LICENSE
 *
 * Copyright (c) 2008-2013 OpenShot Studios, LLC
 * (http://www.openshotstudios.com). This file is part of
 * OpenShot Library (http://www.openshot.org), an open-source project
 * dedicated to delivering high quality video editing and animation solutions
 * to the world.
 *
 * OpenShot Library is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * OpenShot Library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with OpenShot Library.  If not, see <http://www.gnu.org/licenses/>.
 */

#include "../include/KeyFrame.h"

using namespace std;
using namespace openshot;

// Because points can be added in any order, we need to reorder them
// in ascending order based on the point.co.X value.  This simplifies
// processing the curve, due to all the points going from left to right.
void Keyframe::ReorderPoints() {
	// Loop through all coordinates, and sort them by the X attribute
	for (int x = 0; x < Points.size(); x++) {
		int compare_index = x;
		int smallest_index = x;

		for (int compare_index = x + 1; compare_index < Points.size(); compare_index++) {
			if (Points[compare_index].co.X < Points[smallest_index].co.X) {
				smallest_index = compare_index;
			}
		}

		// swap items
		if (smallest_index != compare_index) {
			cout << "swap item " << Points[compare_index].co.X << " with "
					<< Points[smallest_index].co.X << endl;
			swap(Points[compare_index], Points[smallest_index]);
		}
	}
}

// Constructor which sets the default point & coordinate at X=0
Keyframe::Keyframe(float value) : Auto_Handle_Percentage(0.4f), needs_update(true) {
	// Init the factorial table, needed by bezier curves
	CreateFactorialTable();

	// Add initial point
	AddPoint(Point(value));
}

// Keyframe constructor
Keyframe::Keyframe() : Auto_Handle_Percentage(0.4f), needs_update(true) {
	// Init the factorial table, needed by bezier curves
	CreateFactorialTable();
}

// Add a new point on the key-frame.  Each point has a primary coordinate,
// a left handle, and a right handle.
void Keyframe::AddPoint(Point p) {
	// mark as dirty
	needs_update = true;

	// Add point at correct spot
	Points.push_back(p);

	// Sort / Re-order points based on X coordinate
	ReorderPoints();

	// Set Handles (used for smooth curves).
	SetHandles(p);

	// Check if it needs to be processed
	if (needs_update)
		Process();
}

// Add a new point on the key-frame, with some defaults set (BEZIER, AUTO Handles, etc...)
void Keyframe::AddPoint(float x, float y)
{
	// Create a point
	Point new_point(x, y, BEZIER);

	// Add the point
	AddPoint(new_point);
}

// Add a new point on the key-frame, with a specific interpolation type
void Keyframe::AddPoint(float x, float y, InterpolationType interpolate)
{
	// Create a point
	Point new_point(x, y, interpolate);

	// Add the point
	AddPoint(new_point);
}

// Set the handles, used for smooth curves.  The handles are based
// on the surrounding points.
void Keyframe::SetHandles(Point current)
{
	// mark as dirty
	needs_update = true;

	// Lookup the index of this point
	int index = FindIndex(current);
	Point *Current_Point = &Points[index];

	// Find the previous point and next points (if any)
	Point *Previous_Point = NULL;
	Point *Next_Point = NULL;
	float Previous_X_diff = 0.0f;
	float Next_X_diff = 0.0f;

	// If not the 1st point
	if (index > 0)
		Previous_Point = &Points[index - 1];

	// If not the last point
	if (index < (Points.size() - 1))
		Next_Point = &Points[index + 1];

	// Update the previous point's right handle
	if (Previous_Point)
		Previous_X_diff = (Current_Point->co.X - Previous_Point->co.X) * Auto_Handle_Percentage; // Use the keyframe handle percentage to size the handle
	if (Previous_Point && Previous_Point->handle_type == AUTO)
		Previous_Point->handle_right.X = Previous_Point->co.X + Previous_X_diff;
	// Update the current point's left handle
	if (Current_Point->handle_type == AUTO)
		Current_Point->handle_left.X = Current_Point->co.X - Previous_X_diff;

	// Update the next point's left handle
	if (Next_Point)
		Next_X_diff = (Next_Point->co.X - Current_Point->co.X) * Auto_Handle_Percentage; // Use the keyframe handle percentage to size the handle
	if (Next_Point && Next_Point->handle_type == AUTO)
		Next_Point->handle_left.X = Next_Point->co.X - Next_X_diff;
	// Update the next point's right handle
	if (Current_Point->handle_type == AUTO)
		Current_Point->handle_right.X = Current_Point->co.X + Next_X_diff;
}

// Get the index of a point by matching a coordinate
int Keyframe::FindIndex(Point p) throw(OutOfBoundsPoint) {
	// loop through points, and find a matching coordinate
	for (int x = 0; x < Points.size(); x++) {
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

// Get the value at a specific index
float Keyframe::GetValue(int index)
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
int Keyframe::GetInt(int index)
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

// Get the direction of the curve at a specific index (increasing or decreasing)
bool Keyframe::IsIncreasing(int index)
{
	// Check if it needs to be processed
	if (needs_update)
		Process();

	// Is index a valid point?
	if (index >= 0 && index < Values.size())
		// Return value
		return int(round(Values[index].IsIncreasing()));
	else if (index < 0 && Values.size() > 0)
		// Return the minimum value
		return int(round(Values[0].IsIncreasing()));
	else if (index >= Values.size() && Values.size() > 0)
		// return the maximum value
		return int(round(Values[Values.size() - 1].IsIncreasing()));
	else
		// return the default direction of most curves (i.e. increasing is true)
		return true;
}

// Generate JSON of keyframe data
string Keyframe::Json() {

	// Check if it needs to be processed
	if (needs_update)
		Process();

	// Create root json object
	Json::Value root = Json::Value(Json::arrayValue);

	// loop through points, and find a matching coordinate
	for (int x = 0; x < Points.size(); x++) {
		// Get each point
		Point existing_point = Points[x];

		// Create new point object
		Json::Value point = Json::Value(Json::objectValue);
		point["interpolation"] = existing_point.interpolation;
		point["handle_type"] = existing_point.handle_type;
		point["co"] = Json::Value(Json::objectValue);
		point["co"]["X"] = existing_point.co.X;
		point["co"]["Y"] = existing_point.co.Y;

		point["left"] = Json::Value(Json::objectValue);
		point["left"]["X"] = existing_point.handle_left.X;
		point["left"]["Y"] = existing_point.handle_left.Y;

		point["right"] = Json::Value(Json::objectValue);
		point["right"]["X"] = existing_point.handle_right.X;
		point["right"]["Y"] = existing_point.handle_right.Y;

		// Append point to array
		root.append(point);

	}

	// return formatted json string
	return root.toStyledString();

}

// Get the fraction that represents how many times this value is repeated in the curve
Fraction Keyframe::GetRepeatFraction(int index)
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
float Keyframe::GetDelta(int index)
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
Point& Keyframe::GetPoint(int index) throw(OutOfBoundsPoint) {
	// Is index a valid point?
	if (index >= 0 && index < Points.size())
		return Points[index];
	else
		// Invalid index
		throw OutOfBoundsPoint("Invalid point requested", index, Points.size());
}

// Get the number of values (i.e. coordinates on the X axis)
int Keyframe::GetLength() {
	// Check if it needs to be processed
	if (needs_update)
		Process();

	// return the size of the Values vector
	return Values.size();
}

// Remove a point by matching a coordinate
void Keyframe::RemovePoint(Point p) throw(OutOfBoundsPoint) {
	// mark as dirty
	needs_update = true;

	// loop through points, and find a matching coordinate
	for (int x = 0; x < Points.size(); x++) {
		// Get each point
		Point existing_point = Points[x];

		// find a match
		if (p.co.X == existing_point.co.X && p.co.Y == existing_point.co.Y) {
			// Remove the matching point, and break out of loop
			Points.erase(Points.begin() + x);
			break;
		}
	}

	// no matching point found
	throw OutOfBoundsPoint("Invalid point requested", -1, Points.size());
}

// Remove a point by index
void Keyframe::RemovePoint(int index) throw(OutOfBoundsPoint) {
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

void Keyframe::UpdatePoint(int index, Point p) {
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

	for (vector<Point>::iterator it = Points.begin(); it != Points.end(); it++) {
		Point p = *it;
		cout << p.co.X << "\t" << p.co.Y << endl;
	}
}

void Keyframe::PrintValues() {
	// Check if it needs to be processed
	if (needs_update)
		Process();

	cout << "Frame Number (X)\tValue (Y)\tIs Increasing\tRepeat Numerator\tRepeat Denominator\tDelta (Y Difference)" << endl;

	for (vector<Coordinate>::iterator it = Values.begin() + 1; it != Values.end(); it++) {
		Coordinate c = *it;
		cout << int(round(c.X)) << "\t" << c.Y << "\t" << c.IsIncreasing() << "\t" << c.Repeat().num << "\t" << c.Repeat().den << "\t" << c.Delta() << endl;
	}
}

void Keyframe::Process() {
	// only process if needed
	if (!needs_update)
		return;
	else
		// reset flag
		needs_update = false;

	// do not process if no points are found
	if (Points.size() == 0)
		return;

	// Clear all values
	Values.clear();

	// fill in all values between 1 and 1st point's co.X
	Point p1 = Points[0];
	if (Points.size() > 1)
		// Fill in previous X values (before 1st point)
		for (int x = 0; x < p1.co.X; x++)
			Values.push_back(Coordinate(Values.size(), p1.co.Y));
	else
		// Add a single value (since we only have 1 point)
		Values.push_back(Coordinate(Values.size(), p1.co.Y));

	// Loop through each pair of points (1 less than the max points).  Each
	// pair of points is used to process a segment of the keyframe.
	Point p2(0, 0);
	for (int x = 0; x < Points.size() - 1; x++) {
		p1 = Points[x];
		p2 = Points[x + 1];

		// process segment p1,p2
		ProcessSegment(x, p1, p2);
	}

	// Loop through each Value, and set the direction of the coordinate.  This is used
	// when time mapping, to determine what direction the audio waveforms play.
	bool increasing = true;
	int repeat_count = 1;
	int last_value = 0;
	for (vector<Coordinate>::iterator it = Values.begin() + 1; it != Values.end(); it++) {
		int current_value = int(round((*it).Y));
		int next_value = int(round((*it).Y));
		int prev_value = int(round((*it).Y));
		if (it + 1 != Values.end())
			next_value = int(round((*(it + 1)).Y));
		if (it - 1 >= Values.begin())
			prev_value = int(round((*(it - 1)).Y));

		// Loop forward and look for the next unique value (to determine direction)
		for (vector<Coordinate>::iterator direction_it = it + 1; direction_it != Values.end(); direction_it++) {
			int next = int(round((*direction_it).Y));

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
			int next = int(round((*repeat_it).Y));
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

void Keyframe::ProcessSegment(int Segment, Point p1, Point p2) {
	// Determine the number of values for this segment
	int number_of_values = round(p2.co.X) - round(p1.co.X);

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
		float current_value = p1.co.Y;
		float value_difference = p2.co.Y - p1.co.Y;
		float value_increment = 0.0f;

		// Get the increment value, but take into account the
		// first segment has 1 extra value
		value_increment = value_difference / (float) (number_of_values);

		if (Segment == 0)
			// Add an extra value to the first segment
			number_of_values++;
		else
			// If not 1st segment, skip the first value
			current_value += value_increment;

		// Add each increment to the values vector
		for (int x = 0; x < number_of_values; x++) {
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

		vector<Coordinate> segment_coordinates;
		segment_coordinates.push_back(p1.co);
		segment_coordinates.push_back(p1.handle_right);
		segment_coordinates.push_back(p2.handle_left);
		segment_coordinates.push_back(p2.co);

		vector<Coordinate> raw_coordinates;
		int npts = segment_coordinates.size();
		int icount, jcount;
		double step, t;
		double last_x = -1; // small number init, to track the last used x

		// Calculate points on curve
		icount = 0;
		t = 0;

		step = (double) 1.0 / (number_of_values - 1);

		for (int i1 = 0; i1 < number_of_values; i1++) {
			if ((1.0 - t) < 5e-6)
				t = 1.0;

			jcount = 0;

			float new_x = 0.0f;
			float new_y = 0.0f;

			for (int i = 0; i < npts; i++) {
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
		int current_frame = p1.co.X;
		float current_value = p1.co.Y;
		for (int i = 0; i < raw_coordinates.size(); i++)
		{
			// Get the raw coordinate
			Coordinate raw = raw_coordinates[i];

			if (current_frame == round(raw.X))
				// get value of raw coordinate
				current_value = raw.Y;
			else
			{
				// Missing X values (use last known Y values)
				int number_of_missing = round(raw.X) - current_frame;
				for (int missing = 0; missing < number_of_missing; missing++)
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
		for (int x = 0; x < number_of_values; x++) {
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
double Keyframe::Factorial(int n) {
	assert(n >= 0 && n <= 3);
	return FactorialLookup[n]; /* returns the value n! as a SUMORealing point number */
}

// Calculate the factorial function for Bernstein basis
double Keyframe::Ni(int n, int i) {
	double ni;
	double a1 = Factorial(n);
	double a2 = Factorial(i);
	double a3 = Factorial(n - i);
	ni = a1 / (a2 * a3);
	return ni;
}

// Calculate Bernstein basis
double Keyframe::Bernstein(int n, int i, double t) {
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

