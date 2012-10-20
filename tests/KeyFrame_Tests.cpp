#include "UnitTest++.h"
#include "../include/OpenShot.h"

using namespace std;
using namespace openshot;

TEST(Keyframe_GetPoint_With_No_Points)
{
	// Create an empty keyframe
	Keyframe k1;

	CHECK_THROW(k1.GetPoint(0), OutOfBoundsPoint);
}

TEST(Keyframe_GetPoint_With_1_Points)
{
	// Create an empty keyframe
	Keyframe k1;
	k1.AddPoint(openshot::Point(2,3));

	CHECK_THROW(k1.GetPoint(-1), OutOfBoundsPoint);
	CHECK_EQUAL(1, k1.Points.size());
	CHECK_CLOSE(2.0f, k1.GetPoint(0).co.X, 0.00001);
	CHECK_CLOSE(3.0f, k1.GetPoint(0).co.Y, 0.00001);
	CHECK_THROW(k1.GetPoint(1), OutOfBoundsPoint);
}


TEST(Keyframe_AddPoint_With_1_Point)
{
	// Create an empty keyframe
	Keyframe k1;
	k1.Auto_Handle_Percentage = 0.4f;
	k1.AddPoint(openshot::Point(2,9));

	CHECK_CLOSE(2.0f, k1.GetPoint(0).co.X, 0.00001);
	CHECK_THROW(k1.GetPoint(-1), OutOfBoundsPoint);
	CHECK_THROW(k1.GetPoint(1), OutOfBoundsPoint);
}

TEST(Keyframe_AddPoint_With_2_Points)
{
	// Create an empty keyframe
	Keyframe k1;
	k1.Auto_Handle_Percentage = 0.4f;
	k1.AddPoint(openshot::Point(2,9));
	k1.AddPoint(openshot::Point(5,20));

	CHECK_CLOSE(2.0f, k1.GetPoint(0).co.X, 0.00001);
	CHECK_CLOSE(5.0f, k1.GetPoint(1).co.X, 0.00001);
	CHECK_THROW(k1.GetPoint(-1), OutOfBoundsPoint);
	CHECK_THROW(k1.GetPoint(2), OutOfBoundsPoint);
}

TEST(Keyframe_GetValue_For_Bezier_Curve_2_Points)
{
	// Create a keyframe curve with 2 points
	Keyframe kf;
	kf.Auto_Handle_Percentage = 0.4f;
	kf.AddPoint(openshot::Point(Coordinate(1, 1), BEZIER));
	kf.AddPoint(openshot::Point(Coordinate(50, 4), BEZIER));

	// Spot check values from the curve
	CHECK_CLOSE(1.0f, kf.GetValue(-1), 0.0001);
	CHECK_CLOSE(1.0f, kf.GetValue(0), 0.0001);
	CHECK_CLOSE(1.00023f, kf.GetValue(1), 0.0001);
	CHECK_CLOSE(1.18398f, kf.GetValue(9), 0.0001);
	CHECK_CLOSE(1.99988f, kf.GetValue(20), 0.0001);
	CHECK_CLOSE(3.75424f, kf.GetValue(40), 0.0001);
	CHECK_CLOSE(4.0f, kf.GetValue(50), 0.0001);
	// Check the expected number of values
	CHECK_EQUAL(kf.Values.size(), 51);
}

TEST(Keyframe_GetValue_For_Bezier_Curve_5_Points_40_Percent_Handle)
{
	// Create a keyframe curve with 2 points
	Keyframe kf;
	kf.Auto_Handle_Percentage = 0.4f;
	kf.AddPoint(openshot::Point(Coordinate(1, 1), BEZIER));
	kf.AddPoint(openshot::Point(Coordinate(50, 4), BEZIER));
	kf.AddPoint(openshot::Point(Coordinate(100, 10), BEZIER));
	kf.AddPoint(openshot::Point(Coordinate(150, 0), BEZIER));
	kf.AddPoint(openshot::Point(Coordinate(200, 3), BEZIER));

	// Spot check values from the curve
	CHECK_CLOSE(kf.GetValue(-1), 1.0f, 0.0001);
	CHECK_CLOSE(1.0f, kf.GetValue(0), 0.0001);
	CHECK_CLOSE(1.00023f, kf.GetValue(1), 0.0001);
	CHECK_CLOSE(2.69174f, kf.GetValue(27), 0.0001);
	CHECK_CLOSE(7.46386f, kf.GetValue(77), 0.0001);
	CHECK_CLOSE(4.22691f, kf.GetValue(127), 0.0001);
	CHECK_CLOSE(1.73193f, kf.GetValue(177), 0.0001);
	CHECK_CLOSE(3.0f, kf.GetValue(200), 0.0001);
	// Check the expected number of values
	CHECK_EQUAL(kf.Values.size(), 201);
}

TEST(Keyframe_GetValue_For_Bezier_Curve_5_Points_25_Percent_Handle)
{
	// Create a keyframe curve with 2 points
	Keyframe kf;
	kf.Auto_Handle_Percentage = 0.25f;
	kf.AddPoint(openshot::Point(Coordinate(1, 1), BEZIER));
	kf.AddPoint(openshot::Point(Coordinate(50, 4), BEZIER));
	kf.AddPoint(openshot::Point(Coordinate(100, 10), BEZIER));
	kf.AddPoint(openshot::Point(Coordinate(150, 0), BEZIER));
	kf.AddPoint(openshot::Point(Coordinate(200, 3), BEZIER));

	// Spot check values from the curve
	CHECK_CLOSE(1.0f, kf.GetValue(-1), 0.0001);
	CHECK_CLOSE(1.0f, kf.GetValue(0), 0.0001);
	CHECK_CLOSE(1.0009f, kf.GetValue(1), 0.0001);
	CHECK_CLOSE(2.64678f, kf.GetValue(27), 0.0001);
	CHECK_CLOSE(7.37597f, kf.GetValue(77), 0.0001);
	CHECK_CLOSE(4.37339f, kf.GetValue(127), 0.0001);
	CHECK_CLOSE(1.68798f, kf.GetValue(177), 0.0001);
	CHECK_CLOSE(3.0f, kf.GetValue(200), 0.0001);
	// Check the expected number of values
	CHECK_EQUAL(kf.Values.size(), 201);
}

TEST(Keyframe_GetValue_For_Linear_Curve_3_Points)
{
	// Create a keyframe curve with 2 points
	Keyframe kf;
	kf.Auto_Handle_Percentage = 0.4f;
	kf.AddPoint(openshot::Point(Coordinate(1, 1), LINEAR));
	kf.AddPoint(openshot::Point(Coordinate(25, 8), LINEAR));
	kf.AddPoint(openshot::Point(Coordinate(50, 2), LINEAR));

	// Spot check values from the curve
	CHECK_CLOSE(1.0f, kf.GetValue(-1), 0.0001);
	CHECK_CLOSE(1.0f, kf.GetValue(0), 0.0001);
	CHECK_CLOSE(1.0f, kf.GetValue(1), 0.0001);
	CHECK_CLOSE(3.33333f, kf.GetValue(9), 0.0001);
	CHECK_CLOSE(6.54167f, kf.GetValue(20), 0.0001);
	CHECK_CLOSE(4.4f, kf.GetValue(40), 0.0001);
	CHECK_CLOSE(2.0f, kf.GetValue(50), 0.0001);
	// Check the expected number of values
	CHECK_EQUAL(kf.Values.size(), 51);
}

TEST(Keyframe_GetValue_For_Constant_Curve_3_Points)
{
	// Create a keyframe curve with 2 points
	Keyframe kf;
	kf.Auto_Handle_Percentage = 0.4f;
	kf.AddPoint(openshot::Point(Coordinate(1, 1), CONSTANT));
	kf.AddPoint(openshot::Point(Coordinate(25, 8), CONSTANT));
	kf.AddPoint(openshot::Point(Coordinate(50, 2), CONSTANT));

	// Spot check values from the curve
	CHECK_CLOSE(1.0f, kf.GetValue(-1), 0.0001);
	CHECK_CLOSE(1.0f, kf.GetValue(0), 0.0001);
	CHECK_CLOSE(1.0f, kf.GetValue(1), 0.0001);
	CHECK_CLOSE(1.0f, kf.GetValue(24), 0.0001);
	CHECK_CLOSE(8.0f, kf.GetValue(25), 0.0001);
	CHECK_CLOSE(8.0f, kf.GetValue(40), 0.0001);
	CHECK_CLOSE(8.0f, kf.GetValue(49), 0.0001);
	CHECK_CLOSE(2.0f, kf.GetValue(50), 0.0001);
	// Check the expected number of values
	CHECK_EQUAL(kf.Values.size(), 51);
}

TEST(Keyframe_Check_Direction_and_Repeat_Fractions)
{
	// Create a keyframe curve with 2 points
	Keyframe kf;
	kf.AddPoint(1, 500);
	kf.AddPoint(400, 100);
	kf.AddPoint(500, 500);

	// Spot check values from the curve
	CHECK_EQUAL(kf.GetInt(1), 500);
	CHECK_EQUAL(kf.IsIncreasing(1), false);
	CHECK_EQUAL(kf.GetRepeatFraction(1).num, 1);
	CHECK_EQUAL(kf.GetRepeatFraction(1).den, 10);

	CHECK_EQUAL(kf.GetInt(24), 497);
	CHECK_EQUAL(kf.IsIncreasing(24), false);
	CHECK_EQUAL(kf.GetRepeatFraction(24).num, 2);
	CHECK_EQUAL(kf.GetRepeatFraction(24).den, 4);

	CHECK_EQUAL(kf.GetInt(390), 101);
	CHECK_EQUAL(kf.IsIncreasing(390), false);
	CHECK_EQUAL(kf.GetRepeatFraction(390).num, 8);
	CHECK_EQUAL(kf.GetRepeatFraction(390).den, 8);

	CHECK_EQUAL(kf.GetInt(391), 100);
	CHECK_EQUAL(kf.IsIncreasing(391), true);
	CHECK_EQUAL(kf.GetRepeatFraction(391).num, 1);
	CHECK_EQUAL(kf.GetRepeatFraction(391).den, 12);
}
