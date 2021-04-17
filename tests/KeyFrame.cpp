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

#include <catch2/catch.hpp>

#include "KeyFrame.h"
#include "Exceptions.h"
#include "Coordinate.h"
#include "Fraction.h"
#include "Point.h"

using namespace openshot;

TEST_CASE( "GetPoint_With_No_Points", "[libopenshot][keyframe]" )
{
	// Create an empty keyframe
	Keyframe k1;

	CHECK_THROWS_AS(k1.GetPoint(0), OutOfBoundsPoint);
}

TEST_CASE( "GetPoint_With_1_Points", "[libopenshot][keyframe]" )
{
	// Create an empty keyframe
	Keyframe k1;
	k1.AddPoint(openshot::Point(2,3));

	CHECK_THROWS_AS(k1.GetPoint(-1), OutOfBoundsPoint);
	CHECK(k1.GetCount() == 1);
	CHECK(k1.GetPoint(0).co.X == Approx(2.0f).margin(0.00001));
	CHECK(k1.GetPoint(0).co.Y == Approx(3.0f).margin(0.00001));
	CHECK_THROWS_AS(k1.GetPoint(1), OutOfBoundsPoint);
}


TEST_CASE( "AddPoint_With_1_Point", "[libopenshot][keyframe]" )
{
	// Create an empty keyframe
	Keyframe k1;
	k1.AddPoint(openshot::Point(2,9));

	CHECK(k1.GetPoint(0).co.X == Approx(2.0f).margin(0.00001));
	CHECK_THROWS_AS(k1.GetPoint(-1), OutOfBoundsPoint);
	CHECK_THROWS_AS(k1.GetPoint(1), OutOfBoundsPoint);
}

TEST_CASE( "AddPoint_With_2_Points", "[libopenshot][keyframe]" )
{
	// Create an empty keyframe
	Keyframe k1;
	k1.AddPoint(openshot::Point(2,9));
	k1.AddPoint(openshot::Point(5,20));

	CHECK(k1.GetPoint(0).co.X == Approx(2.0f).margin(0.00001));
	CHECK(k1.GetPoint(1).co.X == Approx(5.0f).margin(0.00001));
	CHECK_THROWS_AS(k1.GetPoint(-1), OutOfBoundsPoint);
	CHECK_THROWS_AS(k1.GetPoint(2), OutOfBoundsPoint);
}

TEST_CASE( "GetValue_For_Bezier_Curve_2_Points", "[libopenshot][keyframe]" )
{
	// Create a keyframe curve with 2 points
	Keyframe kf;
	kf.AddPoint(openshot::Point(Coordinate(1, 1), BEZIER));
	kf.AddPoint(openshot::Point(Coordinate(50, 4), BEZIER));

	// Spot check values from the curve
	CHECK(kf.GetValue(-1) == Approx(1.0f).margin(0.0001));
	CHECK(kf.GetValue(0) == Approx(1.0f).margin(0.0001));
	CHECK(kf.GetValue(1) == Approx(1.0f).margin(0.0001));
	CHECK(kf.GetValue(9) == Approx(1.12414f).margin(0.0001));
	CHECK(kf.GetValue(20) == Approx(1.86370f).margin(0.0001));
	CHECK(kf.GetValue(40) == Approx(3.79733f).margin(0.0001));
	CHECK(kf.GetValue(50) == Approx(4.0f).margin(0.0001));
	// Check the expected number of values
	CHECK(kf.GetLength() == 51);
}

TEST_CASE( "GetValue_For_Bezier_Curve_5_Points_40_Percent_Handle", "[libopenshot][keyframe]" )
{
	// Create a keyframe curve with 2 points
	Keyframe kf;
	kf.AddPoint(openshot::Point(Coordinate(1, 1), BEZIER));
	kf.AddPoint(openshot::Point(Coordinate(50, 4), BEZIER));
	kf.AddPoint(openshot::Point(Coordinate(100, 10), BEZIER));
	kf.AddPoint(openshot::Point(Coordinate(150, 0), BEZIER));
	kf.AddPoint(openshot::Point(Coordinate(200, 3), BEZIER));

	// Spot check values from the curve
	CHECK(1.0f == Approx(kf.GetValue(-1)).margin(0.0001));
	CHECK(kf.GetValue(0) == Approx(1.0f).margin(0.0001));
	CHECK(kf.GetValue(1) == Approx(1.0f).margin(0.0001));
	CHECK(kf.GetValue(27) == Approx(2.68197f).margin(0.0001));
	CHECK(kf.GetValue(77) == Approx(7.47719f).margin(0.0001));
	CHECK(kf.GetValue(127) == Approx(4.20468f).margin(0.0001));
	CHECK(kf.GetValue(177) == Approx(1.73860f).margin(0.0001));
	CHECK(kf.GetValue(200) == Approx(3.0f).margin(0.0001));
	// Check the expected number of values
	CHECK(kf.GetLength() == 201);
}

TEST_CASE( "GetValue_For_Bezier_Curve_5_Points_25_Percent_Handle", "[libopenshot][keyframe]" )
{
	// Create a keyframe curve with 2 points
	Keyframe kf;
	kf.AddPoint(openshot::Point(Coordinate(1, 1), BEZIER));
	kf.AddPoint(openshot::Point(Coordinate(50, 4), BEZIER));
	kf.AddPoint(openshot::Point(Coordinate(100, 10), BEZIER));
	kf.AddPoint(openshot::Point(Coordinate(150, 0), BEZIER));
	kf.AddPoint(openshot::Point(Coordinate(200, 3), BEZIER));

	// Spot check values from the curve
	CHECK(kf.GetValue(-1) == Approx(1.0f).margin(0.0001));
	CHECK(kf.GetValue(0) == Approx(1.0f).margin(0.0001));
	CHECK(kf.GetValue(1) == Approx(1.0f).margin(0.0001));
	CHECK(kf.GetValue(27) == Approx(2.68197f).margin(0.0001));
	CHECK(kf.GetValue(77) == Approx(7.47719f).margin(0.0001));
	CHECK(kf.GetValue(127) == Approx(4.20468f).margin(0.0001));
	CHECK(kf.GetValue(177) == Approx(1.73860f).margin(0.0001));
	CHECK(kf.GetValue(200) == Approx(3.0f).margin(0.0001));
	// Check the expected number of values
	CHECK(kf.GetLength() == 201);
}

TEST_CASE( "GetValue_For_Linear_Curve_3_Points", "[libopenshot][keyframe]" )
{
	// Create a keyframe curve with 2 points
	Keyframe kf;
	kf.AddPoint(openshot::Point(Coordinate(1, 1), LINEAR));
	kf.AddPoint(openshot::Point(Coordinate(25, 8), LINEAR));
	kf.AddPoint(openshot::Point(Coordinate(50, 2), LINEAR));

	// Spot check values from the curve
	CHECK(kf.GetValue(-1) == Approx(1.0f).margin(0.0001));
	CHECK(kf.GetValue(0) == Approx(1.0f).margin(0.0001));
	CHECK(kf.GetValue(1) == Approx(1.0f).margin(0.0001));
	CHECK(kf.GetValue(9) == Approx(3.33333f).margin(0.0001));
	CHECK(kf.GetValue(20) == Approx(6.54167f).margin(0.0001));
	CHECK(kf.GetValue(40) == Approx(4.4f).margin(0.0001));
	CHECK(kf.GetValue(50) == Approx(2.0f).margin(0.0001));
	// Check the expected number of values
	CHECK(kf.GetLength() == 51);
}

TEST_CASE( "GetValue_For_Constant_Curve_3_Points", "[libopenshot][keyframe]" )
{
	// Create a keyframe curve with 2 points
	Keyframe kf;
	kf.AddPoint(openshot::Point(Coordinate(1, 1), CONSTANT));
	kf.AddPoint(openshot::Point(Coordinate(25, 8), CONSTANT));
	kf.AddPoint(openshot::Point(Coordinate(50, 2), CONSTANT));

	// Spot check values from the curve
	CHECK(kf.GetValue(-1) == Approx(1.0f).margin(0.0001));
	CHECK(kf.GetValue(0) == Approx(1.0f).margin(0.0001));
	CHECK(kf.GetValue(1) == Approx(1.0f).margin(0.0001));
	CHECK(kf.GetValue(24) == Approx(1.0f).margin(0.0001));
	CHECK(kf.GetValue(25) == Approx(8.0f).margin(0.0001));
	CHECK(kf.GetValue(40) == Approx(8.0f).margin(0.0001));
	CHECK(kf.GetValue(49) == Approx(8.0f).margin(0.0001));
	CHECK(kf.GetValue(50) == Approx(2.0f).margin(0.0001));
	// Check the expected number of values
	CHECK(kf.GetLength() == 51);
}

TEST_CASE( "Check_Direction_and_Repeat_Fractions", "[libopenshot][keyframe]" )
{
	// Create a keyframe curve with 2 points
	Keyframe kf;
	kf.AddPoint(1, 500);
	kf.AddPoint(400, 100);
	kf.AddPoint(500, 500);

	// Spot check values from the curve
	CHECK(kf.GetInt(1) == 500);
	CHECK_FALSE(kf.IsIncreasing(1));
	CHECK(kf.GetRepeatFraction(1).num == 1);
	CHECK(kf.GetRepeatFraction(1).den == 13);
	CHECK(kf.GetDelta(1) == 500);

	CHECK(kf.GetInt(24) == 498);
	CHECK_FALSE(kf.IsIncreasing(24));
	CHECK(kf.GetRepeatFraction(24).num == 3);
	CHECK(kf.GetRepeatFraction(24).den == 6);
	CHECK(kf.GetDelta(24) == 0);

	CHECK(kf.GetLong(390) == 100);
	CHECK(kf.IsIncreasing(390) == true);
	CHECK(kf.GetRepeatFraction(390).num == 3);
	CHECK(kf.GetRepeatFraction(390).den == 16);
	CHECK(kf.GetDelta(390) == 0);

	CHECK(kf.GetLong(391) == 100);
	CHECK(kf.IsIncreasing(391) == true);
	CHECK(kf.GetRepeatFraction(391).num == 4);
	CHECK(kf.GetRepeatFraction(391).den == 16);
	CHECK(kf.GetDelta(388) == -1);
}


TEST_CASE( "Get_Closest_Point", "[libopenshot][keyframe]" )
{
	// Create a keyframe curve with 2 points
	Keyframe kf;
	kf.AddPoint(1, 0.0);
	kf.AddPoint(1000, 1.0);
	kf.AddPoint(2500, 0.0);

	// Spot check values from the curve (to the right)
	CHECK(kf.GetClosestPoint(openshot::Point(900, 900)).co.X == 1000);
	CHECK(kf.GetClosestPoint(openshot::Point(1, 1)).co.X == 1);
	CHECK(kf.GetClosestPoint(openshot::Point(5, 5)).co.X == 1000);
	CHECK(kf.GetClosestPoint(openshot::Point(1000, 1000)).co.X == 1000);
	CHECK(kf.GetClosestPoint(openshot::Point(1001, 1001)).co.X == 2500);
	CHECK(kf.GetClosestPoint(openshot::Point(2500, 2500)).co.X == 2500);
	CHECK(kf.GetClosestPoint(openshot::Point(3000, 3000)).co.X == 2500);

	// Spot check values from the curve (to the left)
	CHECK(kf.GetClosestPoint(openshot::Point(900, 900), true).co.X == 1);
	CHECK(kf.GetClosestPoint(openshot::Point(1, 1), true).co.X == 1);
	CHECK(kf.GetClosestPoint(openshot::Point(5, 5), true).co.X == 1);
	CHECK(kf.GetClosestPoint(openshot::Point(1000, 1000), true).co.X == 1);
	CHECK(kf.GetClosestPoint(openshot::Point(1001, 1001), true).co.X == 1000);
	CHECK(kf.GetClosestPoint(openshot::Point(2500, 2500), true).co.X == 1000);
	CHECK(kf.GetClosestPoint(openshot::Point(3000, 3000), true).co.X == 2500);
}


TEST_CASE( "Get_Previous_Point", "[libopenshot][keyframe]" )
{
	// Create a keyframe curve with 2 points
	Keyframe kf;
	kf.AddPoint(1, 0.0);
	kf.AddPoint(1000, 1.0);
	kf.AddPoint(2500, 0.0);

	// Spot check values from the curve
	CHECK(kf.GetPreviousPoint(kf.GetClosestPoint(openshot::Point(900, 900))).co.X == 1);
	CHECK(kf.GetPreviousPoint(kf.GetClosestPoint(openshot::Point(1, 1))).co.X == 1);
	CHECK(kf.GetPreviousPoint(kf.GetClosestPoint(openshot::Point(5, 5))).co.X == 1);
	CHECK(kf.GetPreviousPoint(kf.GetClosestPoint(openshot::Point(1000, 1000))).co.X == 1);
	CHECK(kf.GetPreviousPoint(kf.GetClosestPoint(openshot::Point(1001, 1001))).co.X == 1000);
	CHECK(kf.GetPreviousPoint(kf.GetClosestPoint(openshot::Point(2500, 2500))).co.X == 1000);
	CHECK(kf.GetPreviousPoint(kf.GetClosestPoint(openshot::Point(3000, 3000))).co.X == 1000);

}

TEST_CASE( "Get_Max_Point", "[libopenshot][keyframe]" )
{
	// Create a keyframe curve
	Keyframe kf;
	kf.AddPoint(1, 1.0);

	// Spot check values from the curve
	CHECK(kf.GetMaxPoint().co.Y == 1.0);

	kf.AddPoint(2, 0.0);

	// Spot check values from the curve
	CHECK(kf.GetMaxPoint().co.Y == 1.0);

	kf.AddPoint(3, 2.0);

	// Spot check values from the curve
	CHECK(kf.GetMaxPoint().co.Y == 2.0);

	kf.AddPoint(4, 1.0);

	// Spot check values from the curve
	CHECK(kf.GetMaxPoint().co.Y == 2.0);
}

TEST_CASE( "Scale_Keyframe", "[libopenshot][keyframe]" )
{
	// Create a keyframe curve with 2 points
	Keyframe kf;
	kf.AddPoint(openshot::Point(Coordinate(1, 1), BEZIER));
	kf.AddPoint(openshot::Point(Coordinate(25, 8), BEZIER));
	kf.AddPoint(openshot::Point(Coordinate(50, 2), BEZIER));

	// Spot check values from the curve
	CHECK(kf.GetValue(1) == Approx(1.0f).margin(0.01));
	CHECK(kf.GetValue(24) == Approx(7.99f).margin(0.01));
	CHECK(kf.GetValue(25) == Approx(8.0f).margin(0.01));
	CHECK(kf.GetValue(40) == Approx(3.85f).margin(0.01));
	CHECK(kf.GetValue(49) == Approx(2.01f).margin(0.01));
	CHECK(kf.GetValue(50) == Approx(2.0f).margin(0.01));

	// Resize / Scale the keyframe
	kf.ScalePoints(2.0); // 100% larger

	// Spot check values from the curve
	CHECK(kf.GetValue(1) == Approx(1.0f).margin(0.01));
	CHECK(kf.GetValue(24) == Approx(4.08f).margin(0.01));
	CHECK(kf.GetValue(25) == Approx(4.36f).margin(0.01));
	CHECK(kf.GetValue(40) == Approx(7.53f).margin(0.01));
	CHECK(kf.GetValue(49) == Approx(7.99f).margin(0.01));
	CHECK(kf.GetValue(50) == Approx(8.0f).margin(0.01));
	CHECK(kf.GetValue(90) == Approx(2.39f).margin(0.01));
	CHECK(kf.GetValue(100) == Approx(2.0f).margin(0.01));

	// Resize / Scale the keyframe
	kf.ScalePoints(0.5); // 50% smaller, which should match the original size

	// Spot check values from the curve
	CHECK(kf.GetValue(1) == Approx(1.0f).margin(0.01));
	CHECK(kf.GetValue(24) == Approx(7.99f).margin(0.01));
	CHECK(kf.GetValue(25) == Approx(8.0f).margin(0.01));
	CHECK(kf.GetValue(40) == Approx(3.85f).margin(0.01));
	CHECK(kf.GetValue(49) == Approx(2.01f).margin(0.01));
	CHECK(kf.GetValue(50) == Approx(2.0f).margin(0.01));

}

TEST_CASE( "Flip_Keyframe", "[libopenshot][keyframe]" )
{
	// Create a keyframe curve with 2 points
	Keyframe kf;
	kf.AddPoint(openshot::Point(Coordinate(1, 1), LINEAR));
	kf.AddPoint(openshot::Point(Coordinate(25, 8), LINEAR));
	kf.AddPoint(openshot::Point(Coordinate(50, 2), LINEAR));
	kf.AddPoint(openshot::Point(Coordinate(100, 10), LINEAR));

	// Spot check values from the curve
	CHECK(kf.GetValue(1) == Approx(1.0f).margin(0.01));
	CHECK(kf.GetValue(25) == Approx(8.0f).margin(0.01));
	CHECK(kf.GetValue(50) == Approx(2.0f).margin(0.01));
	CHECK(kf.GetValue(100) == Approx(10.0f).margin(0.01));

	// Flip the points
	kf.FlipPoints();

	// Spot check values from the curve
	CHECK(kf.GetValue(1) == Approx(10.0f).margin(0.01));
	CHECK(kf.GetValue(25) == Approx(2.0f).margin(0.01));
	CHECK(kf.GetValue(50) == Approx(8.0f).margin(0.01));
	CHECK(kf.GetValue(100) == Approx(1.0f).margin(0.01));

	// Flip the points again (back to the original)
	kf.FlipPoints();

	// Spot check values from the curve
	CHECK(kf.GetValue(1) == Approx(1.0f).margin(0.01));
	CHECK(kf.GetValue(25) == Approx(8.0f).margin(0.01));
	CHECK(kf.GetValue(50) == Approx(2.0f).margin(0.01));
	CHECK(kf.GetValue(100) == Approx(10.0f).margin(0.01));
}

TEST_CASE( "Remove_Duplicate_Point", "[libopenshot][keyframe]" )
{
	// Create a keyframe curve with 2 points
	Keyframe kf;
	kf.AddPoint(1, 0.0);
	kf.AddPoint(1, 1.0);
	kf.AddPoint(1, 2.0);

	// Spot check values from the curve
	CHECK(kf.GetLength() == 1);
	CHECK(kf.GetPoint(0).co.Y == Approx(2.0).margin(0.01));
}

TEST_CASE( "Large_Number_Values", "[libopenshot][keyframe]" )
{
	// Large value
	int64_t const large_value = 30 * 60 * 90;

	// Create a keyframe curve with 2 points
	Keyframe kf;
	kf.AddPoint(1, 1.0);
	kf.AddPoint(large_value, 100.0); // 90 minutes long

	// Spot check values from the curve
	CHECK(kf.GetLength() == large_value + 1);
	CHECK(kf.GetPoint(0).co.Y == Approx(1.0).margin(0.01));
	CHECK(kf.GetPoint(1).co.Y == Approx(100.0).margin(0.01));
}

TEST_CASE( "Remove_Point", "[libopenshot][keyframe]" )
{
	Keyframe kf;
	kf.AddPoint(openshot::Point(Coordinate(1, 1), CONSTANT));
	kf.AddPoint(openshot::Point(Coordinate(3, 100), CONSTANT));
	CHECK(kf.GetInt(2) == 1);
	kf.AddPoint(openshot::Point(Coordinate(2, 50), CONSTANT));
	CHECK(kf.GetInt(2) == 50);
	kf.RemovePoint(1); // This is the index of point with X == 2
	CHECK(kf.GetInt(2) == 1);
	CHECK_THROWS_AS(kf.RemovePoint(100), OutOfBoundsPoint);
}

TEST_CASE( "Constant_Interpolation_First_Segment", "[libopenshot][keyframe]" )
{
	Keyframe kf;
	kf.AddPoint(Point(Coordinate(1, 1), CONSTANT));
	kf.AddPoint(Point(Coordinate(2, 50), CONSTANT));
	kf.AddPoint(Point(Coordinate(3, 100), CONSTANT));
	CHECK(kf.GetInt(0) == 1);
	CHECK(kf.GetInt(1) == 1);
	CHECK(kf.GetInt(2) == 50);
	CHECK(kf.GetInt(3) == 100);
	CHECK(kf.GetInt(4) == 100);
}

TEST_CASE( "isIncreasing", "[libopenshot][keyframe]" )
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
	CHECK(kf.IsIncreasing(0) == true);
	CHECK(kf.IsIncreasing(15) == true);
	// all next equal
	CHECK_FALSE(kf.IsIncreasing(12));
	// first non-eq is larger
	CHECK(kf.IsIncreasing(8) == true);
	// first non-eq is smaller
	CHECK_FALSE(kf.IsIncreasing(6));
	// bezier and linear
	CHECK(kf.IsIncreasing(4) == true);
	CHECK(kf.IsIncreasing(2) == true);
}

TEST_CASE( "GetLength", "[libopenshot][keyframe]" )
{
	Keyframe f;
	CHECK(f.GetLength() == 0);
	f.AddPoint(1, 1);
	CHECK(f.GetLength() == 1);
	f.AddPoint(2, 1);
	CHECK(f.GetLength() == 3);
	f.AddPoint(200, 1);
	CHECK(f.GetLength() == 201);

	Keyframe g;
	g.AddPoint(200, 1);
	CHECK(g.GetLength() == 1);
	g.AddPoint(1,1);
	CHECK(g.GetLength() == 201);
}

TEST_CASE( "Use_Interpolation_of_Segment_End_Point", "[libopenshot][keyframe]" )
{
	Keyframe f;
	f.AddPoint(1,0, CONSTANT);
	f.AddPoint(100,155, BEZIER);
	CHECK(f.GetValue(50) == Approx(75.9).margin(0.1));
}

TEST_CASE( "Handle_Large_Segment", "[libopenshot][keyframe]" )
{
	Keyframe kf;
	kf.AddPoint(1, 0, CONSTANT);
	kf.AddPoint(1000000, 1, LINEAR);

	CHECK(kf.GetValue(500000) == Approx(0.5).margin(0.01));
	CHECK(kf.IsIncreasing(10) == true);

	Fraction fr = kf.GetRepeatFraction(250000);
	CHECK((double)fr.num / fr.den == Approx(0.5).margin(0.01));
}

TEST_CASE( "Point_Vector_Constructor", "[libopenshot][keyframe]" )
{
	std::vector<Point> points{Point(1, 10), Point(5, 20), Point(10, 30)};
	Keyframe k1(points);

	CHECK(k1.GetLength() == 11);
	CHECK(k1.GetValue(10) == Approx(30.0f).margin(0.0001));
}
