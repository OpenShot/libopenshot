/**
 * @file
 * @brief Unit tests for openshot::Point
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

#include "UnitTest++.h"
#include "../include/OpenShot.h"

using namespace std;
using namespace openshot;

TEST(Point_Default_Constructor)
{
	// Create a point with X and Y values
	openshot::Point p1(2,9);

	CHECK_EQUAL(2, p1.co.X);
	CHECK_EQUAL(9, p1.co.Y);
	CHECK_EQUAL(BEZIER, p1.interpolation);
}

TEST(Point_Constructor_With_Coordinate)
{
	// Create a point with a coordinate
	Coordinate c1(3,7);
	openshot::Point p1(c1);

	CHECK_EQUAL(3, p1.co.X);
	CHECK_EQUAL(7, p1.co.Y);
	CHECK_EQUAL(BEZIER, p1.interpolation);
}

TEST(Point_Constructor_With_Coordinate_And_LINEAR_Interpolation)
{
	// Create a point with a coordinate and interpolation
	Coordinate c1(3,9);
	InterpolationType interp = LINEAR;
	openshot::Point p1(c1, interp);

	CHECK_EQUAL(3, c1.X);
	CHECK_EQUAL(9, c1.Y);
	CHECK_EQUAL(LINEAR, p1.interpolation);
}

TEST(Point_Constructor_With_Coordinate_And_BEZIER_Interpolation)
{
	// Create a point with a coordinate and interpolation
	Coordinate c1(3,9);
	InterpolationType interp = BEZIER;
	openshot::Point p1(c1, interp);

	CHECK_EQUAL(3, p1.co.X);
	CHECK_EQUAL(9, p1.co.Y);
	CHECK_EQUAL(BEZIER, p1.interpolation);
}

TEST(Point_Constructor_With_Coordinate_And_CONSTANT_Interpolation)
{
	// Create a point with a coordinate and interpolation
	Coordinate c1(2,8);
	InterpolationType interp = CONSTANT;
	openshot::Point p1(c1, interp);

	CHECK_EQUAL(2, p1.co.X);
	CHECK_EQUAL(8, p1.co.Y);
	CHECK_EQUAL(CONSTANT, p1.interpolation);
}

TEST(Point_Constructor_With_Coordinate_And_BEZIER_And_AUTO_Handle)
{
	// Create a point with a coordinate and interpolation
	Coordinate c1(3,9);
	openshot::Point p1(c1, BEZIER, AUTO);

	CHECK_EQUAL(3, p1.co.X);
	CHECK_EQUAL(9, p1.co.Y);
	CHECK_EQUAL(BEZIER, p1.interpolation);
	CHECK_EQUAL(AUTO, p1.handle_type);
}

TEST(Point_Constructor_With_Coordinate_And_BEZIER_And_MANUAL_Handle)
{
	// Create a point with a coordinate and interpolation
	Coordinate c1(3,9);
	openshot::Point p1(c1, BEZIER, MANUAL);

	CHECK_EQUAL(3, p1.co.X);
	CHECK_EQUAL(9, p1.co.Y);
	CHECK_EQUAL(BEZIER, p1.interpolation);
	CHECK_EQUAL(MANUAL, p1.handle_type);
}
