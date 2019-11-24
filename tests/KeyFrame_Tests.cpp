/**
 * @file
 * @brief Unit tests for openshot::Keyframe
 * @author Jonathan Thomas <jonathan@openshot.org>
 *
 * @ref License
 */

/* LICENSE
 *
 * Copyright (c) 2008-2019 OpenShot Studios, LLC
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
// Prevent name clashes with juce::UnitTest
#define DONT_SET_USING_JUCE_NAMESPACE 1
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
	CHECK_EQUAL(1, k1.GetCount());
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
	CHECK_CLOSE(1.0f, kf.GetValue(1), 0.0001);
	CHECK_CLOSE(1.12414f, kf.GetValue(9), 0.0001);
	CHECK_CLOSE(1.86370f, kf.GetValue(20), 0.0001);
	CHECK_CLOSE(3.79733f, kf.GetValue(40), 0.0001);
	CHECK_CLOSE(4.0f, kf.GetValue(50), 0.0001);
	// Check the expected number of values
	CHECK_EQUAL(kf.GetLength(), 51);
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
	CHECK_CLOSE(1.0f, kf.GetValue(1), 0.0001);
	CHECK_CLOSE(2.68197f, kf.GetValue(27), 0.0001);
	CHECK_CLOSE(7.47719f, kf.GetValue(77), 0.0001);
	CHECK_CLOSE(4.20468f, kf.GetValue(127), 0.0001);
	CHECK_CLOSE(1.73860f, kf.GetValue(177), 0.0001);
	CHECK_CLOSE(3.0f, kf.GetValue(200), 0.0001);
	// Check the expected number of values
	CHECK_EQUAL(kf.GetLength(), 201);
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
	CHECK_CLOSE(1.0f, kf.GetValue(1), 0.0001);
	CHECK_CLOSE(2.68197f, kf.GetValue(27), 0.0001);
	CHECK_CLOSE(7.47719f, kf.GetValue(77), 0.0001);
	CHECK_CLOSE(4.20468f, kf.GetValue(127), 0.0001);
	CHECK_CLOSE(1.73860f, kf.GetValue(177), 0.0001);
	CHECK_CLOSE(3.0f, kf.GetValue(200), 0.0001);
	// Check the expected number of values
	CHECK_EQUAL(kf.GetLength(), 201);
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
	CHECK_EQUAL(kf.GetLength(), 51);
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
	CHECK_EQUAL(kf.GetLength(), 51);
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
	CHECK_EQUAL(kf.GetRepeatFraction(1).den, 13);
	CHECK_EQUAL(kf.GetDelta(1), 500);

	CHECK_EQUAL(kf.GetInt(24), 498);
	CHECK_EQUAL(kf.IsIncreasing(24), false);
	CHECK_EQUAL(kf.GetRepeatFraction(24).num, 3);
	CHECK_EQUAL(kf.GetRepeatFraction(24).den, 6);
	CHECK_EQUAL(kf.GetDelta(24), 0);

	CHECK_EQUAL(kf.GetLong(390), 100);
	CHECK_EQUAL(kf.IsIncreasing(390), true);
	CHECK_EQUAL(kf.GetRepeatFraction(390).num, 3);
	CHECK_EQUAL(kf.GetRepeatFraction(390).den, 16);
	CHECK_EQUAL(kf.GetDelta(390), 0);

	CHECK_EQUAL(kf.GetLong(391), 100);
	CHECK_EQUAL(kf.IsIncreasing(391), true);
	CHECK_EQUAL(kf.GetRepeatFraction(391).num, 4);
	CHECK_EQUAL(kf.GetRepeatFraction(391).den, 16);
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
	CHECK_CLOSE(3.85f, kf.GetValue(40), 0.01);
	CHECK_CLOSE(2.01f, kf.GetValue(49), 0.01);
	CHECK_CLOSE(2.0f, kf.GetValue(50), 0.01);

	// Resize / Scale the keyframe
	kf.ScalePoints(2.0); // 100% larger

	// Spot check values from the curve
	CHECK_CLOSE(1.0f, kf.GetValue(1), 0.01);
	CHECK_CLOSE(4.08f, kf.GetValue(24), 0.01);
	CHECK_CLOSE(4.36f, kf.GetValue(25), 0.01);
	CHECK_CLOSE(7.53f, kf.GetValue(40), 0.01);
	CHECK_CLOSE(7.99f, kf.GetValue(49), 0.01);
	CHECK_CLOSE(8.0f, kf.GetValue(50), 0.01);
	CHECK_CLOSE(2.39f, kf.GetValue(90), 0.01);
	CHECK_CLOSE(2.0f, kf.GetValue(100), 0.01);

	// Resize / Scale the keyframe
	kf.ScalePoints(0.5); // 50% smaller, which should match the original size

	// Spot check values from the curve
	CHECK_CLOSE(1.0f, kf.GetValue(1), 0.01);
	CHECK_CLOSE(7.99f, kf.GetValue(24), 0.01);
	CHECK_CLOSE(8.0f, kf.GetValue(25), 0.01);
	CHECK_CLOSE(3.85f, kf.GetValue(40), 0.01);
	CHECK_CLOSE(2.01f, kf.GetValue(49), 0.01);
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

TEST(Keyframe_Large_Number_Values)
{
	// Large value
	int64_t large_value = 30 * 60 * 90;

	// Create a keyframe curve with 2 points
	Keyframe kf;
	kf.AddPoint(1, 1.0);
	kf.AddPoint(large_value, 100.0); // 90 minutes long

	// Spot check values from the curve
	CHECK_EQUAL(kf.GetLength(), large_value + 1);
	CHECK_CLOSE(kf.GetPoint(0).co.Y, 1.0, 0.01);
	CHECK_CLOSE(kf.GetPoint(1).co.Y, 100.0, 0.01);
}

TEST(Keyframe_Remove_Point)
{
	Keyframe kf;
	kf.AddPoint(openshot::Point(Coordinate(1, 1), CONSTANT));
	kf.AddPoint(openshot::Point(Coordinate(3, 100), CONSTANT));
	CHECK_EQUAL(1, kf.GetInt(2));
	kf.AddPoint(openshot::Point(Coordinate(2, 50), CONSTANT));
	CHECK_EQUAL(50, kf.GetInt(2));
	kf.RemovePoint(1); // This is the index of point with X == 2
	CHECK_EQUAL(1, kf.GetInt(2));
	CHECK_THROW(kf.RemovePoint(100), OutOfBoundsPoint);
}

TEST(Keyframe_Constant_Interpolation_First_Segment)
{
	Keyframe kf;
	kf.AddPoint(Point(Coordinate(1, 1), CONSTANT));
	kf.AddPoint(Point(Coordinate(2, 50), CONSTANT));
	kf.AddPoint(Point(Coordinate(3, 100), CONSTANT));
	CHECK_EQUAL(1, kf.GetInt(0));
	CHECK_EQUAL(1, kf.GetInt(1));
	CHECK_EQUAL(50, kf.GetInt(2));
	CHECK_EQUAL(100, kf.GetInt(3));
	CHECK_EQUAL(100, kf.GetInt(4));
}

TEST(Keyframe_isIncreasing)
{
	// Which cases need to be tested to keep same behaviour as
	// previously?
	//
	// - "invalid point" => true
	// - point where all next values are equal => false
	// - point where first non-eq next value is smaller => false
	// - point where first non-eq next value is larger => true
	Keyframe kf;
	kf.AddPoint(1, 1, LINEAR); // testing with linear
	kf.AddPoint(3, 5, BEZIER); // testing with bezier
	kf.AddPoint(6, 10, CONSTANT); // first non-eq is smaller
	kf.AddPoint(8, 8, CONSTANT); // first non-eq is larger
	kf.AddPoint(10, 10, CONSTANT); // all next values are equal
	kf.AddPoint(15, 10, CONSTANT);

	// "invalid points"
	CHECK_EQUAL(true, kf.IsIncreasing(0));
	CHECK_EQUAL(true, kf.IsIncreasing(15));
	// all next equal
	CHECK_EQUAL(false, kf.IsIncreasing(12));
	// first non-eq is larger
	CHECK_EQUAL(true, kf.IsIncreasing(8));
	// first non-eq is smaller
	CHECK_EQUAL(false, kf.IsIncreasing(6));
	// bezier and linear
	CHECK_EQUAL(true, kf.IsIncreasing(4));
	CHECK_EQUAL(true, kf.IsIncreasing(2));
}

TEST(Keyframe_GetLength)
{
	Keyframe f;
	CHECK_EQUAL(0, f.GetLength());
	f.AddPoint(1, 1);
	CHECK_EQUAL(1, f.GetLength());
	f.AddPoint(2, 1);
	CHECK_EQUAL(3, f.GetLength());
	f.AddPoint(200, 1);
	CHECK_EQUAL(201, f.GetLength());

	Keyframe g;
	g.AddPoint(200, 1);
	CHECK_EQUAL(1, g.GetLength());
	g.AddPoint(1,1);
	CHECK_EQUAL(201, g.GetLength());
}

TEST(Keyframe_Use_Interpolation_of_Segment_End_Point)
{
	Keyframe f;
	f.AddPoint(1,0, CONSTANT);
	f.AddPoint(100,155, BEZIER);
	CHECK_CLOSE(75.9, f.GetValue(50), 0.1);
}
