/**
 * \file
 * \brief Source code for the Point class
 * \author Copyright (c) 2008-2013 OpenShot Studios, LLC
 */

#include "../include/Point.h"

using namespace std;
using namespace openshot;

// Constructor which creates a single coordinate at X=0
Point::Point(float y) :
	interpolation(BEZIER), handle_type(AUTO) {
	// set new coorinate
	co = Coordinate(0, y);

	// set handles
	Initialize_Handles();
}

Point::Point(float x, float y) :
	interpolation(BEZIER), handle_type(AUTO) {
	// set new coorinate
	co = Coordinate(x, y);

	// set handles
	Initialize_Handles();
}

// Constructor which also creates a Point and sets the X,Y, and interpolation of the Point.
Point::Point(float x, float y, Interpolation_Type interpolation) :
	handle_type(AUTO), interpolation(interpolation) {
	// set new coorinate
	co = Coordinate(x, y);

	// set handles
	Initialize_Handles();
}

Point::Point(Coordinate co) :
	co(co), interpolation(BEZIER), handle_type(AUTO) {
	// set handles
	Initialize_Handles();
}

Point::Point(Coordinate co, Interpolation_Type interpolation) :
	co(co), interpolation(interpolation), handle_type(AUTO) {
	// set handles
	Initialize_Handles();
}

Point::Point(Coordinate co, Interpolation_Type interpolation, Handle_Type handle_type) :
	co(co), interpolation(interpolation), handle_type(handle_type) {
	// set handles
	Initialize_Handles();
}

void Point::Initialize_Handles(float Offset) {
	// initialize left and right handles
	handle_left = Coordinate(co.X - Offset, co.Y);
	handle_right = Coordinate(co.X + Offset, co.Y);
}
