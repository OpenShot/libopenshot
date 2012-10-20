/**
 * \file
 * \brief Source code for the Coordinate class
 * \author Copyright (c) 2011 Jonathan Thomas
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
