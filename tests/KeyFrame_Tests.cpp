/**
 * @file
 * @brief Unit tests for openshot::Keyframe
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
	k1.AddPoint(openshot::Point(2,9));

	CHECK_CLOSE(2.0f, k1.GetPoint(0).co.X, 0.00001);
	CHECK_THROW(k1.GetPoint(-1), OutOfBoundsPoint);
	CHECK_THROW(k1.GetPoint(1), OutOfBoundsPoint);
}

TEST(Keyframe_AddPoint_With_2_Points)
{
	// Create an empty keyframe
	Keyframe k1;
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
	kf.AddPoint(openshot::Point(Coordinate(1, 1), BEZIER));
	kf.AddPoint(openshot::Point(Coordinate(50, 4), BEZIER));

	// Spot check values from the curve
	CHECK_CLOSE(1.0f, kf.GetValue(-1), 0.0001);
	CHECK_CLOSE(1.0f, kf.GetValue(0), 0.0001);
	CHECK_CLOSE(1.00023f, kf.GetValue(1), 0.0001);
	CHECK_CLOSE(1.14025f, kf.GetValue(9), 0.0001);
	CHECK_CLOSE(1.91492f, kf.GetValue(20), 0.0001);
	CHECK_CLOSE(3.81602f, kf.GetValue(40), 0.0001);
	CHECK_CLOSE(4.0f, kf.GetValue(50), 0.0001);
	// Check the expected number of values
	CHECK_EQUAL(kf.Values.size(), 51);
}

TEST(Keyframe_GetValue_For_Bezier_Curve_5_Points_40_Percent_Handle)
{
	// Create a keyframe curve with 2 points
	Keyframe kf;
	kf.AddPoint(openshot::Point(Coordinate(1, 1), BEZIER));
	kf.AddPoint(openshot::Point(Coordinate(50, 4), BEZIER));
	kf.AddPoint(openshot::Point(Coordinate(100, 10), BEZIER));
	kf.AddPoint(openshot::Point(Coordinate(150, 0), BEZIER));
	kf.AddPoint(openshot::Point(Coordinate(200, 3), BEZIER));

	// Spot check values from the curve
	CHECK_CLOSE(kf.GetValue(-1), 1.0f, 0.0001);
	CHECK_CLOSE(1.0f, kf.GetValue(0), 0.0001);
	CHECK_CLOSE(1.00023f, kf.GetValue(1), 0.0001);
	CHECK_CLOSE(2.73656f, kf.GetValue(27), 0.0001);
	CHECK_CLOSE(7.55139f, kf.GetValue(77), 0.0001);
	CHECK_CLOSE(4.08102f, kf.GetValue(127), 0.0001);
	CHECK_CLOSE(1.77569f, kf.GetValue(177), 0.0001);
	CHECK_CLOSE(3.0f, kf.GetValue(200), 0.0001);
	// Check the expected number of values
	CHECK_EQUAL(kf.Values.size(), 201);
}

TEST(Keyframe_GetValue_For_Bezier_Curve_5_Points_25_Percent_Handle)
{
	// Create a keyframe curve with 2 points
	Keyframe kf;
	kf.AddPoint(openshot::Point(Coordinate(1, 1), BEZIER));
	kf.AddPoint(openshot::Point(Coordinate(50, 4), BEZIER));
	kf.AddPoint(openshot::Point(Coordinate(100, 10), BEZIER));
	kf.AddPoint(openshot::Point(Coordinate(150, 0), BEZIER));
	kf.AddPoint(openshot::Point(Coordinate(200, 3), BEZIER));

	// Spot check values from the curve
	CHECK_CLOSE(1.0f, kf.GetValue(-1), 0.0001);
	CHECK_CLOSE(1.0f, kf.GetValue(0), 0.0001);
	CHECK_CLOSE(1.00023f, kf.GetValue(1), 0.0001);
	CHECK_CLOSE(2.73656f, kf.GetValue(27), 0.0001);
	CHECK_CLOSE(7.55139f, kf.GetValue(77), 0.0001);
	CHECK_CLOSE(4.08102f, kf.GetValue(127), 0.0001);
	CHECK_CLOSE(1.77569f, kf.GetValue(177), 0.0001);
	CHECK_CLOSE(3.0f, kf.GetValue(200), 0.0001);
	// Check the expected number of values
	CHECK_EQUAL(kf.Values.size(), 201);
}

TEST(Keyframe_GetValue_For_Linear_Curve_3_Points)
{
	// Create a keyframe curve with 2 points
	Keyframe kf;
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
	CHECK_EQUAL(kf.GetRepeatFraction(1).den, 12);
	CHECK_EQUAL(kf.GetDelta(1), 500);

	CHECK_EQUAL(kf.GetInt(24), 498);
	CHECK_EQUAL(kf.IsIncreasing(24), false);
	CHECK_EQUAL(kf.GetRepeatFraction(24).num, 3);
	CHECK_EQUAL(kf.GetRepeatFraction(24).den, 6);
	CHECK_EQUAL(kf.GetDelta(24), 0);

	CHECK_EQUAL(kf.GetLong(390), 100);
	CHECK_EQUAL(kf.IsIncreasing(390), true);
	CHECK_EQUAL(kf.GetRepeatFraction(390).num, 3);
	CHECK_EQUAL(kf.GetRepeatFraction(390).den, 15);
	CHECK_EQUAL(kf.GetDelta(390), 0);

	CHECK_EQUAL(kf.GetLong(391), 100);
	CHECK_EQUAL(kf.IsIncreasing(391), true);
	CHECK_EQUAL(kf.GetRepeatFraction(391).num, 4);
	CHECK_EQUAL(kf.GetRepeatFraction(391).den, 15);
	CHECK_EQUAL(kf.GetDelta(388), -1);
}


TEST(Keyframe_Get_Closest_Point)
{
	// Create a keyframe curve with 2 points
	Keyframe kf;
	kf.AddPoint(1, 0.0);
	kf.AddPoint(1000, 1.0);
	kf.AddPoint(2500, 0.0);

	// Spot check values from the curve (to the right)
	CHECK_EQUAL(kf.GetClosestPoint(openshot::Point(900, 900)).co.X, 1000);
	CHECK_EQUAL(kf.GetClosestPoint(openshot::Point(1, 1)).co.X, 1);
	CHECK_EQUAL(kf.GetClosestPoint(openshot::Point(5, 5)).co.X, 1000);
	CHECK_EQUAL(kf.GetClosestPoint(openshot::Point(1000, 1000)).co.X, 1000);
	CHECK_EQUAL(kf.GetClosestPoint(openshot::Point(1001, 1001)).co.X, 2500);
	CHECK_EQUAL(kf.GetClosestPoint(openshot::Point(2500, 2500)).co.X, 2500);
	CHECK_EQUAL(kf.GetClosestPoint(openshot::Point(3000, 3000)).co.X, 2500);

	// Spot check values from the curve (to the left)
	CHECK_EQUAL(kf.GetClosestPoint(openshot::Point(900, 900), true).co.X, 1);
	CHECK_EQUAL(kf.GetClosestPoint(openshot::Point(1, 1), true).co.X, 1);
	CHECK_EQUAL(kf.GetClosestPoint(openshot::Point(5, 5), true).co.X, 1);
	CHECK_EQUAL(kf.GetClosestPoint(openshot::Point(1000, 1000), true).co.X, 1);
	CHECK_EQUAL(kf.GetClosestPoint(openshot::Point(1001, 1001), true).co.X, 1000);
	CHECK_EQUAL(kf.GetClosestPoint(openshot::Point(2500, 2500), true).co.X, 1000);
	CHECK_EQUAL(kf.GetClosestPoint(openshot::Point(3000, 3000), true).co.X, 2500);
}


TEST(Keyframe_Get_Previous_Point)
{
	// Create a keyframe curve with 2 points
	Keyframe kf;
	kf.AddPoint(1, 0.0);
	kf.AddPoint(1000, 1.0);
	kf.AddPoint(2500, 0.0);

	// Spot check values from the curve
	CHECK_EQUAL(kf.GetPreviousPoint(kf.GetClosestPoint(openshot::Point(900, 900))).co.X, 1);
	CHECK_EQUAL(kf.GetPreviousPoint(kf.GetClosestPoint(openshot::Point(1, 1))).co.X, 1);
	CHECK_EQUAL(kf.GetPreviousPoint(kf.GetClosestPoint(openshot::Point(5, 5))).co.X, 1);
	CHECK_EQUAL(kf.GetPreviousPoint(kf.GetClosestPoint(openshot::Point(1000, 1000))).co.X, 1);
	CHECK_EQUAL(kf.GetPreviousPoint(kf.GetClosestPoint(openshot::Point(1001, 1001))).co.X, 1000);
	CHECK_EQUAL(kf.GetPreviousPoint(kf.GetClosestPoint(openshot::Point(2500, 2500))).co.X, 1000);
	CHECK_EQUAL(kf.GetPreviousPoint(kf.GetClosestPoint(openshot::Point(3000, 3000))).co.X, 1000);

}

TEST(Keyframe_Get_Max_Point)
{
	// Create a keyframe curve
	Keyframe kf;
	kf.AddPoint(1, 1.0);

	// Spot check values from the curve
	CHECK_EQUAL(kf.GetMaxPoint().co.Y, 1.0);

	kf.AddPoint(2, 0.0);

	// Spot check values from the curve
	CHECK_EQUAL(kf.GetMaxPoint().co.Y, 1.0);

	kf.AddPoint(3, 2.0);

	// Spot check values from the curve
	CHECK_EQUAL(kf.GetMaxPoint().co.Y, 2.0);

	kf.AddPoint(4, 1.0);

	// Spot check values from the curve
	CHECK_EQUAL(kf.GetMaxPoint().co.Y, 2.0);
}

TEST(Keyframe_Scale_Keyframe)
{
	// Create a keyframe curve with 2 points
	Keyframe kf;
	kf.AddPoint(openshot::Point(Coordinate(1, 1), BEZIER));
	kf.AddPoint(openshot::Point(Coordinate(25, 8), BEZIER));
	kf.AddPoint(openshot::Point(Coordinate(50, 2), BEZIER));

	// Spot check values from the curve
	CHECK_CLOSE(1.0f, kf.GetValue(1), 0.01);
	CHECK_CLOSE(7.99f, kf.GetValue(24), 0.01);
	CHECK_CLOSE(8.0f, kf.GetValue(25), 0.01);
	CHECK_CLOSE(3.68f, kf.GetValue(40), 0.01);
	CHECK_CLOSE(2.0f, kf.GetValue(49), 0.01);
	CHECK_CLOSE(2.0f, kf.GetValue(50), 0.01);

	// Resize / Scale the keyframe
	kf.ScalePoints(2.0); // 100% larger

	// Spot check values from the curve
	CHECK_CLOSE(1.0f, kf.GetValue(1), 0.01);
	CHECK_CLOSE(4.21f, kf.GetValue(24), 0.01);
	CHECK_CLOSE(4.47f, kf.GetValue(25), 0.01);
	CHECK_CLOSE(7.57f, kf.GetValue(40), 0.01);
	CHECK_CLOSE(7.99f, kf.GetValue(49), 0.01);
	CHECK_CLOSE(8.0f, kf.GetValue(50), 0.01);
	CHECK_CLOSE(2.35f, kf.GetValue(90), 0.01);
	CHECK_CLOSE(2.0f, kf.GetValue(100), 0.01);

	// Resize / Scale the keyframe
	kf.ScalePoints(0.5); // 50% smaller, which should match the original size

	// Spot check values from the curve
	CHECK_CLOSE(1.0f, kf.GetValue(1), 0.01);
	CHECK_CLOSE(7.99f, kf.GetValue(24), 0.01);
	CHECK_CLOSE(8.0f, kf.GetValue(25), 0.01);
	CHECK_CLOSE(3.68f, kf.GetValue(40), 0.01);
	CHECK_CLOSE(2.0f, kf.GetValue(49), 0.01);
	CHECK_CLOSE(2.0f, kf.GetValue(50), 0.01);

}

TEST(Keyframe_Flip_Keyframe)
{
	// Create a keyframe curve with 2 points
	Keyframe kf;
	kf.AddPoint(openshot::Point(Coordinate(1, 1), LINEAR));
	kf.AddPoint(openshot::Point(Coordinate(25, 8), LINEAR));
	kf.AddPoint(openshot::Point(Coordinate(50, 2), LINEAR));
	kf.AddPoint(openshot::Point(Coordinate(100, 10), LINEAR));

	// Spot check values from the curve
	CHECK_CLOSE(1.0f, kf.GetValue(1), 0.01);
	CHECK_CLOSE(8.0f, kf.GetValue(25), 0.01);
	CHECK_CLOSE(2.0f, kf.GetValue(50), 0.01);
	CHECK_CLOSE(10.0f, kf.GetValue(100), 0.01);

	// Flip the points
	kf.FlipPoints();

	// Spot check values from the curve
	CHECK_CLOSE(10.0f, kf.GetValue(1), 0.01);
	CHECK_CLOSE(2.0f, kf.GetValue(25), 0.01);
	CHECK_CLOSE(8.0f, kf.GetValue(50), 0.01);
	CHECK_CLOSE(1.0f, kf.GetValue(100), 0.01);

	// Flip the points again (back to the original)
	kf.FlipPoints();

	// Spot check values from the curve
	CHECK_CLOSE(1.0f, kf.GetValue(1), 0.01);
	CHECK_CLOSE(8.0f, kf.GetValue(25), 0.01);
	CHECK_CLOSE(2.0f, kf.GetValue(50), 0.01);
	CHECK_CLOSE(10.0f, kf.GetValue(100), 0.01);
}

TEST(Keyframe_Remove_Duplicate_Point)
{
	// Create a keyframe curve with 2 points
	Keyframe kf;
	kf.AddPoint(1, 0.0);
	kf.AddPoint(1, 1.0);
	kf.AddPoint(1, 2.0);

	// Spot check values from the curve
	CHECK_EQUAL(kf.GetLength(), 1);
	CHECK_CLOSE(kf.GetPoint(0).co.Y, 2.0, 0.01);
}