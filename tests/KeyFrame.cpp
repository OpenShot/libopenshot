/**
 * @file
 * @brief Unit tests for openshot::Keyframe
 * @author Jonathan Thomas <jonathan@openshot.org>
 *
 * @ref License
 */

// Copyright (c) 2008-2019 OpenShot Studios, LLC
//
// SPDX-License-Identifier: LGPL-3.0-or-later

#include "openshot_catch.h"

#include <sstream>
#include <memory>

#include "KeyFrame.h"
#include "Coordinate.h"
#include "Clip.h"
#include "Exceptions.h"
#include "FFmpegReader.h"
#include "Fraction.h"
#include "Point.h"
#include "Timeline.h"

#ifdef USE_OPENCV
#include "effects/Tracker.h"
#include "TrackedObjectBBox.h"
#endif

using namespace openshot;

TEST_CASE( "GetPoint (no Points)", "[libopenshot][keyframe]" )
{
	// Create an empty keyframe
	Keyframe k1;

	CHECK_THROWS_AS(k1.GetPoint(0), OutOfBoundsPoint);
}

TEST_CASE( "GetPoint (1 Point)", "[libopenshot][keyframe]" )
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


TEST_CASE( "AddPoint (1 Point)", "[libopenshot][keyframe]" )
{
	// Create an empty keyframe
	Keyframe k1;
	k1.AddPoint(openshot::Point(2,9));

	CHECK(k1.GetPoint(0).co.X == Approx(2.0f).margin(0.00001));
	CHECK_THROWS_AS(k1.GetPoint(-1), OutOfBoundsPoint);
	CHECK_THROWS_AS(k1.GetPoint(1), OutOfBoundsPoint);
}

TEST_CASE( "AddPoint (2 Points)", "[libopenshot][keyframe]" )
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

TEST_CASE( "GetValue (Bezier curve, 2 Points)", "[libopenshot][keyframe]" )
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
	CHECK(kf.GetLength() == 50);
}

TEST_CASE( "GetValue (Bezier, 5 Points, 40% handle)", "[libopenshot][keyframe]" )
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
	CHECK(kf.GetLength() == 200);
}

TEST_CASE( "GetValue (Bezier, 5 Points, 25% Handle)", "[libopenshot][keyframe]" )
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
	CHECK(kf.GetLength() == 200);
}

TEST_CASE( "GetValue (Linear, 3 Points)", "[libopenshot][keyframe]" )
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
	CHECK(kf.GetLength() == 50);
}

TEST_CASE( "GetValue (Constant, 3 Points)", "[libopenshot][keyframe]" )
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
	CHECK(kf.GetLength() == 50);
}

TEST_CASE( "GetDelta", "[libopenshot][keyframe]" )
{
	// Create a keyframe curve with 2 points
	Keyframe kf;
	kf.AddPoint(1, 500);
	kf.AddPoint(400, 100);
	kf.AddPoint(500, 500);

	// Spot check values from the curve
	CHECK(kf.GetInt(1) == 500);
	CHECK_FALSE(kf.IsIncreasing(1));
	CHECK(kf.GetDelta(1) == 500);

	CHECK(kf.GetInt(24) == 498);
	CHECK_FALSE(kf.IsIncreasing(24));
	CHECK(kf.GetDelta(24) == Approx(-0.1622f).margin(0.0001));

	CHECK(kf.GetLong(390) == 100);
	CHECK(kf.IsIncreasing(390) == false);
	CHECK(kf.GetDelta(390) == Approx(-0.0732f).margin(0.0001));

	CHECK(kf.GetLong(391) == 100);
	CHECK(kf.IsIncreasing(391) == false);
	CHECK(kf.GetDelta(388) == Approx(-0.0886f).margin(0.0001));
}


TEST_CASE( "GetClosestPoint", "[libopenshot][keyframe]" )
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


TEST_CASE( "GetPreviousPoint", "[libopenshot][keyframe]" )
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

TEST_CASE( "GetMaxPoint", "[libopenshot][keyframe]" )
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

TEST_CASE( "Keyframe scaling", "[libopenshot][keyframe]" )
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

TEST_CASE( "flip Keyframe", "[libopenshot][keyframe]" )
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

TEST_CASE( "remove duplicate Point", "[libopenshot][keyframe]" )
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

TEST_CASE( "large number values", "[libopenshot][keyframe]" )
{
	// Large value
	int64_t const large_value = 30 * 60 * 90;

	// Create a keyframe curve with 2 points
	Keyframe kf;
	kf.AddPoint(1, 1.0);
	kf.AddPoint(large_value, 100.0); // 90 minutes long

	// Spot check values from the curve
	CHECK(kf.GetLength() == large_value);
	CHECK(kf.GetPoint(0).co.Y == Approx(1.0).margin(0.01));
	CHECK(kf.GetPoint(1).co.Y == Approx(100.0).margin(0.01));
}

TEST_CASE( "remove Point", "[libopenshot][keyframe]" )
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

TEST_CASE( "Constant interp, first segment", "[libopenshot][keyframe]" )
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

TEST_CASE( "IsIncreasing", "[libopenshot][keyframe]" )
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
	CHECK(kf.IsIncreasing(12) == true);
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
	CHECK(f.GetLength() == 2);
	f.AddPoint(200, 1);
	CHECK(f.GetLength() == 200);

	Keyframe g;
	g.AddPoint(200, 1);
	CHECK(g.GetLength() == 1);
	g.AddPoint(1,1);
	CHECK(g.GetLength() == 200);
}

TEST_CASE( "use segment end point interpolation", "[libopenshot][keyframe]" )
{
	Keyframe f;
	f.AddPoint(1,0, CONSTANT);
	f.AddPoint(100,155, BEZIER);
	CHECK(f.GetValue(50) == Approx(75.9).margin(0.1));
}

TEST_CASE( "handle large segment", "[libopenshot][keyframe]" )
{
	Keyframe kf;
	kf.AddPoint(1, 0, CONSTANT);
	kf.AddPoint(1000000, 1, LINEAR);

	CHECK(kf.GetValue(500000) == Approx(0.5).margin(0.01));
	CHECK(kf.IsIncreasing(10) == true);
}

TEST_CASE( "std::vector<Point> constructor", "[libopenshot][keyframe]" )
{
	std::vector<Point> points{Point(1, 10), Point(5, 20), Point(10, 30)};
	Keyframe k1(points);

	CHECK(k1.GetLength() == 10);
	CHECK(k1.GetValue(10) == Approx(30.0f).margin(0.0001));
}

TEST_CASE( "PrintPoints", "[libopenshot][keyframe]" )
{
	std::vector<Point> points{
		Point(1, 10),
		Point(225, 397),
		Point(430, -153.4),
		Point(999, 12345.678)
	};
	Keyframe k1(points);

	std::stringstream output;
	k1.PrintPoints(&output);

    const std::string expected =
R"(     1       10.0000
   225      397.0000
   430     -153.4000
   999    12345.6777)";

	// Ensure the two strings are equal up to the limits of 'expected'
	CHECK(output.str().substr(0, expected.size()) == expected);
}

TEST_CASE( "PrintValues", "[libopenshot][keyframe]" )
{
	std::vector<Point> points{
		Point(1, 10),
		Point(225, 397),
		Point(430, -153.4),
		Point(999, 12345.678)
	};
	Keyframe k1(points);

	std::stringstream output;
	k1.PrintValues(&output);

    const std::string expected =
R"(│Frame# (X) │     Y Value │ Delta Y │ Increasing?│
├───────────┼─────────────┼─────────┼────────────┤
│       1 * │     10.0000 │     +10 │        true│
│       2   │     10.0104 │+0.01036 │        true│
│       3   │     10.0414 │+0.03101 │        true│
│       4   │     10.0942 │+0.05279 │        true│
│       5   │     10.1665 │+0.07234 │        true│
│       6   │     10.2633 │+0.09682 │        true│
│       7   │     10.3794 │ +0.1161 │        true│
│       8   │     10.5193 │ +0.1399 │        true│
│       9   │     10.6807 │ +0.1614 │        true│
│      10   │     10.8636 │ +0.1828 │        true│
│      11   │     11.0719 │ +0.2083 │        true│
│      12   │     11.3021 │ +0.2303 │        true│
│      13   │     11.5542 │ +0.2521 │        true│
│      14   │     11.8334 │ +0.2792 │        true│
│      15   │     12.1349 │ +0.3015 │        true│
│      16   │     12.4587 │ +0.3237 │        true│
│      17   │     12.8111 │ +0.3525 │        true│
│      18   │     13.1863 │ +0.3752 │        true│
│      19   │     13.5840 │ +0.3977 │        true│
│      20   │     14.0121 │ +0.4281 │        true│
│      21   │     14.4632 │ +0.4511 │        true│
│      22   │     14.9460 │ +0.4828 │        true│
│      23   │     15.4522 │ +0.5063 │        true│
│      24   │     15.9818 │ +0.5296 │        true│
│      25   │     16.5446 │ +0.5628 │        true│
│      26   │     17.1312 │ +0.5866 │        true│
│      27   │     17.7414 │ +0.6102 │        true│
│      28   │     18.3862 │ +0.6449 │        true│
│      29   │     19.0551 │ +0.6689 │        true│
│      30   │     19.7599 │ +0.7048 │        true│
│      31   │     20.4891 │ +0.7292 │        true│
│      32   │     21.2425 │ +0.7534 │        true│
│      33   │     22.0333 │ +0.7908 │        true│
│      34   │     22.8486 │ +0.8153 │        true│
│      35   │     23.7024 │ +0.8539 │        true│
│      36   │     24.5812 │ +0.8788 │        true│)";

	// Ensure the two strings are equal up to the limits of 'expected'
	CHECK(output.str().substr(0, expected.size()) == expected);
}

#ifdef USE_OPENCV
TEST_CASE( "TrackedObjectBBox init", "[libopenshot][keyframe]" )
{
	TrackedObjectBBox kfb(62,143,0,212);

	CHECK(kfb.delta_x.GetInt(1) == 0);
	CHECK(kfb.delta_y.GetInt(1) == 0);

	CHECK(kfb.scale_x.GetInt(1) == 1);
	CHECK(kfb.scale_y.GetInt(1) == 1);

	CHECK(kfb.rotation.GetInt(1) == 0);

	CHECK(kfb.stroke_width.GetInt(1) == 2);
    CHECK(kfb.stroke_alpha.GetValue(1) == Approx(0.7f).margin(0.0001));

	CHECK(kfb.background_alpha .GetInt(1) == 0);
	CHECK(kfb.background_corner.GetInt(1) == 12);

	CHECK(kfb.stroke.red.GetInt(1) == 62);
	CHECK(kfb.stroke.green.GetInt(1) == 143);
	CHECK(kfb.stroke.blue.GetInt(1) == 0);
	CHECK(kfb.stroke.alpha.GetInt(1) == 212);

	CHECK(kfb.background.red.GetInt(1) == 0);
	CHECK(kfb.background.green.GetInt(1) == 0);
	CHECK(kfb.background.blue.GetInt(1) == 255);
	CHECK(kfb.background.alpha.GetInt(1) == 212);
}

TEST_CASE( "TrackedObjectBBox AddBox and RemoveBox", "[libopenshot][keyframe]" )
{
	TrackedObjectBBox kfb;

	kfb.AddBox(1, 10.0, 10.0, 100.0, 100.0, 0.0);

	CHECK(kfb.Contains(1) == true);
	CHECK(kfb.GetLength() == 1);

	kfb.RemoveBox(1);

	CHECK_FALSE(kfb.Contains(1));
	CHECK(kfb.GetLength() == 0);
}

TEST_CASE( "TrackedObjectBBox GetVal", "[libopenshot][keyframe]" )
{
	TrackedObjectBBox kfb;

	kfb.AddBox(1, 10.0, 10.0, 100.0, 100.0, 0.0);

	BBox val = kfb.GetBox(1);

	CHECK(val.cx == 10.0);
	CHECK(val.cy == 10.0);
	CHECK(val.width == 100.0);
	CHECK(val.height == 100.0);
	CHECK(val.angle == 0.0);
}

TEST_CASE( "TrackedObjectBBox GetVal interpolation", "[libopenshot][keyframe]" )
{
	TrackedObjectBBox kfb;

	kfb.AddBox(1, 10.0, 10.0, 100.0, 100.0, 0.0);
	kfb.AddBox(11, 20.0, 20.0, 100.0, 100.0, 0.0);
	kfb.AddBox(21, 30.0, 30.0, 100.0, 100.0, 0.0);
	kfb.AddBox(31, 40.0, 40.0, 100.0, 100.0, 0.0);

	BBox val = kfb.GetBox(5);

	CHECK(val.cx == 14.0);
	CHECK(val.cy == 14.0);
	CHECK(val.width == 100.0);
	CHECK(val.height == 100.0);

	val = kfb.GetBox(15);

	CHECK(val.cx == 24.0);
	CHECK(val.cy == 24.0);
	CHECK(val.width == 100.0);
	CHECK(val.height == 100.0);

	val = kfb.GetBox(25);

	CHECK(val.cx == 34.0);
	CHECK(val.cy == 34.0);
	CHECK(val.width == 100.0);
	CHECK(val.height == 100.0);

}


TEST_CASE( "TrackedObjectBBox SetJson", "[libopenshot][keyframe]" )
{
	TrackedObjectBBox kfb;

	kfb.AddBox(1, 10.0, 10.0, 100.0, 100.0, 0.0);
	kfb.AddBox(10, 20.0, 20.0, 100.0, 100.0, 0.0);
	kfb.AddBox(20, 30.0, 30.0, 100.0, 100.0, 0.0);
	kfb.AddBox(30, 40.0, 40.0, 100.0, 100.0, 0.0);

	kfb.scale_x.AddPoint(1, 2.0);
	kfb.scale_x.AddPoint(10, 3.0);

	kfb.SetBaseFPS(Fraction(24.0, 1.0));

	auto dataJSON = kfb.Json();
	TrackedObjectBBox fromJSON_kfb;
	fromJSON_kfb.SetJson(dataJSON);

	int num_kfb = kfb.GetBaseFPS().num;
	int num_fromJSON_kfb = fromJSON_kfb.GetBaseFPS().num;
	CHECK(num_kfb == num_fromJSON_kfb);

	double time_kfb = kfb.FrameNToTime(1, 1.0);
	double time_fromJSON_kfb = fromJSON_kfb.FrameNToTime(1, 1.0);
	CHECK(time_kfb == time_fromJSON_kfb);

	BBox kfb_bbox =  kfb.BoxVec[time_kfb];
	BBox fromJSON_bbox = fromJSON_kfb.BoxVec[time_fromJSON_kfb];

	CHECK(kfb_bbox.cx == fromJSON_bbox.cx);
	CHECK(kfb_bbox.cy == fromJSON_bbox.cy);
	CHECK(kfb_bbox.width == fromJSON_bbox.width);
	CHECK(kfb_bbox.height == fromJSON_bbox.height);
	CHECK(kfb_bbox.angle == fromJSON_bbox.angle);
}

TEST_CASE( "TrackedObjectBBox scaling", "[libopenshot][keyframe]" )
{
	TrackedObjectBBox kfb;

	kfb.AddBox(1, 10.0, 10.0, 10.0, 10.0, 0.0);
	kfb.scale_x.AddPoint(1.0, 2.0);
	kfb.scale_y.AddPoint(1.0, 3.0);

	BBox bbox = kfb.GetBox(1);

	CHECK(bbox.width == 20.0);
	CHECK(bbox.height == 30.0);
}

TEST_CASE( "AttachToObject", "[libopenshot][keyframe]" )
{
	std::stringstream path1, path2;
	path1 << TEST_MEDIA_PATH << "test.avi";
	path2 << TEST_MEDIA_PATH << "run.mp4";

	// Create Timelime
	Timeline t(1280, 720, Fraction(25,1), 44100, 2, ChannelLayout::LAYOUT_STEREO);

	// Create Clip and add it to the Timeline
	Clip clip(new FFmpegReader(path1.str()));
	clip.Id("AAAA1234");

	// Create a child clip and add it to the Timeline
	Clip childClip(new FFmpegReader(path2.str()));
	childClip.Id("CHILD123");

	// Add clips to timeline
	t.AddClip(&childClip);
	t.AddClip(&clip);

	// Create tracker and add it to clip
	Tracker tracker;
	clip.AddEffect(&tracker);

	// Save a pointer to trackedData
	std::shared_ptr<TrackedObjectBBox> trackedData = tracker.trackedData;

	// Change trackedData scale
	trackedData->scale_x.AddPoint(1, 2.0);
	CHECK(trackedData->scale_x.GetValue(1) == 2.0);

	// Tracked Data JSON
	auto trackedDataJson = trackedData->JsonValue();

	// Get and cast the trakcedObjec
	std::list<std::string> ids = t.GetTrackedObjectsIds();
	auto trackedObject_base = t.GetTrackedObject(ids.front());
	auto trackedObject = std::make_shared<TrackedObjectBBox>();
	trackedObject = std::dynamic_pointer_cast<TrackedObjectBBox>(trackedObject_base);
	CHECK(trackedObject == trackedData);

	// Set trackedObject Json Value
	trackedObject->SetJsonValue(trackedDataJson);

	// Attach childClip to tracked object
	std::string tracked_id = trackedData->Id();
	childClip.Open();
	childClip.AttachToObject(tracked_id);

	auto trackedTest = std::make_shared<TrackedObjectBBox>();
	trackedTest = std::dynamic_pointer_cast<TrackedObjectBBox>(childClip.GetAttachedObject());

	CHECK(trackedData->scale_x.GetValue(1) == trackedTest->scale_x.GetValue(1));

	auto frameTest = childClip.GetFrame(1);
	childClip.Close();
	// XXX: Here, too, there needs to be some sort of actual _testing_ of the results
}

TEST_CASE( "GetBoxValues", "[libopenshot][keyframe]" )
{
	TrackedObjectBBox trackedDataObject;
	trackedDataObject.AddBox(1, 10.0, 10.0, 20.0, 20.0, 30.0);

	auto trackedData = std::make_shared<TrackedObjectBBox>(trackedDataObject);

	auto boxValues = trackedData->GetBoxValues(1);

	CHECK(boxValues["cx"] == 10.0);
	CHECK(boxValues["cy"] == 10.0);
	CHECK(boxValues["w"] == 20.0);
	CHECK(boxValues["h"] == 20.0);
	CHECK(boxValues["ang"] == 30.0);
}
#endif
