/**
 * @file
 * @brief Source file for Coordinate class
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

#include "../include/Coordinate.h"

using namespace std;
using namespace openshot;

// Default constructor for a coordinate, which defaults the X and Y to zero (0,0)
Coordinate::Coordinate() :
	X(0), Y(0), increasing(true), repeated(1,1), delta(0.0) {
}

// Constructor which also allows the user to set the X and Y
Coordinate::Coordinate(float x, float y) :
	X(x), Y(y), increasing(true), repeated(1,1), delta(0.0)  {
}
