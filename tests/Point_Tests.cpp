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
	Interpolation_Type interp = LINEAR;
	openshot::Point p1(c1, interp);

	CHECK_EQUAL(3, c1.X);
	CHECK_EQUAL(9, c1.Y);
	CHECK_EQUAL(LINEAR, p1.interpolation);
}

TEST(Point_Constructor_With_Coordinate_And_BEZIER_Interpolation)
{
	// Create a point with a coordinate and interpolation
	Coordinate c1(3,9);
	Interpolation_Type interp = BEZIER;
	openshot::Point p1(c1, interp);

	CHECK_EQUAL(3, p1.co.X);
	CHECK_EQUAL(9, p1.co.Y);
	CHECK_EQUAL(BEZIER, p1.interpolation);
}

TEST(Point_Constructor_With_Coordinate_And_CONSTANT_Interpolation)
{
	// Create a point with a coordinate and interpolation
	Coordinate c1(2,8);
	Interpolation_Type interp = CONSTANT;
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

TEST(Point_Set_Handles_Auto_No_Value)
{
	// Create a point with X and Y values
	openshot::Point p1(2,8);
	p1.Initialize_Handles();

	CHECK_EQUAL(p1.co.Y, p1.handle_left.Y);
	CHECK_EQUAL(p1.co.Y, p1.handle_right.Y);
	CHECK_CLOSE(p1.co.X, p1.handle_left.X, 0.000001);
	CHECK_CLOSE(p1.co.X, p1.handle_right.X, 0.000001);
}

TEST(Point_Set_Handles_Auto_With_Values)
{
	// Create a point with X and Y values
	openshot::Point p1(2,8);
	p1.Initialize_Handles(4.2);

	CHECK_EQUAL(p1.co.Y, p1.handle_left.Y);
	CHECK_EQUAL(p1.co.Y, p1.handle_right.Y);
	CHECK_CLOSE(p1.co.X - 4.2, p1.handle_left.X, 0.000001);
	CHECK_CLOSE(p1.co.X + 4.2, p1.handle_right.X, 0.000001);
}
