/**
 * @file
 * @brief Source file for Point class
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
Point::Point(float x, float y, InterpolationType interpolation) :
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

Point::Point(Coordinate co, InterpolationType interpolation) :
	co(co), interpolation(interpolation), handle_type(AUTO) {
	// set handles
	Initialize_Handles();
}

Point::Point(Coordinate co, InterpolationType interpolation, HandleType handle_type) :
	co(co), interpolation(interpolation), handle_type(handle_type) {
	// set handles
	Initialize_Handles();
}

void Point::Initialize_Handles(float Offset) {
	// initialize left and right handles
	handle_left = Coordinate(co.X - Offset, co.Y);
	handle_right = Coordinate(co.X + Offset, co.Y);
}
