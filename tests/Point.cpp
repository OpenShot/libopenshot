/**
 * @file
 * @brief Unit tests for openshot::Point
 * @author Jonathan Thomas <jonathan@openshot.org>
 *
 * @ref License
 */

// Copyright (c) 2008-2019 OpenShot Studios, LLC
//
// SPDX-License-Identifier: LGPL-3.0-or-later

#include "openshot_catch.h"
#include <sstream>

#include "Point.h"
#include "Enums.h"
#include "Exceptions.h"
#include "Coordinate.h"
#include "Json.h"


TEST_CASE( "default constructor", "[libopenshot][point]" )
{
	openshot::Point p;

	// Default values
	CHECK(p.co.X == 1);
	CHECK(p.co.Y == 0);
	CHECK(p.handle_left.X == 0.5);
	CHECK(p.handle_left.Y == 1.0);
	CHECK(p.handle_right.X == 0.5);
	CHECK(p.handle_right.Y == 0.0);
	CHECK(p.interpolation == openshot::InterpolationType::BEZIER);
	CHECK(p.handle_type == openshot::HandleType::AUTO);
}
TEST_CASE( "x,y ctor", "[libopenshot][point]" )
{
	// Create a point with X and Y values
	openshot::Point p1(2,9);

	CHECK(p1.co.X == 2);
	CHECK(p1.co.Y == 9);
	CHECK(p1.interpolation == openshot::InterpolationType::BEZIER);
}

TEST_CASE( "std::pair ctor", "[libopenshot][point]" )
{
	// Create a point from a std::pair
	std::pair<double, double> coordinates(22, 5);
	openshot::Point p1(coordinates);

	CHECK(p1.co.X == Approx(22.0f).margin(0.00001));
	CHECK(p1.co.Y == Approx(5.0f).margin(0.00001));
}

TEST_CASE( "Coordinate ctor", "[libopenshot][point]" )
{
	// Create a point with a coordinate
	openshot::Coordinate c1(3,7);
	openshot::Point p1(c1);

	CHECK(p1.co.X == Approx(3.0f).margin(0.00001));
	CHECK(p1.co.Y == Approx(7.0f).margin(0.00001));
	CHECK(p1.interpolation == openshot::InterpolationType::BEZIER);
}

TEST_CASE( "Coordinate ctor, LINEAR", "[libopenshot][point]" )
{
	// Create a point with a coordinate and interpolation
	openshot::Coordinate c1(3,9);
	auto interp = openshot::InterpolationType::LINEAR;
	openshot::Point p1(c1, interp);

	CHECK(c1.X == 3);
	CHECK(c1.Y == 9);
	CHECK(p1.interpolation == openshot::InterpolationType::LINEAR);
}

TEST_CASE( "Coordinate ctor, BEZIER", "[libopenshot][point]" )
{
	// Create a point with a coordinate and interpolation
	openshot::Coordinate c1(3,9);
	auto interp = openshot::InterpolationType::BEZIER;
	openshot::Point p1(c1, interp);

	CHECK(p1.co.X == 3);
	CHECK(p1.co.Y == 9);
	CHECK(p1.interpolation == openshot::InterpolationType::BEZIER);
}

TEST_CASE( "Coordinate ctor, CONSTANT", "[libopenshot][point]" )
{
	// Create a point with a coordinate and interpolation
	openshot::Coordinate c1(2,8);
	auto interp = openshot::InterpolationType::CONSTANT;
	openshot::Point p1(c1, interp);

	CHECK(p1.co.X == 2);
	CHECK(p1.co.Y == 8);
	CHECK(p1.interpolation == openshot::InterpolationType::CONSTANT);
}

TEST_CASE( "Coordinate ctor, BEZIER+AUTO", "[libopenshot][point]" )
{
	// Create a point with a coordinate and interpolation
	openshot::Coordinate c1(3,9);
	openshot::Point p1(c1,
		openshot::InterpolationType::BEZIER,
		openshot::HandleType::AUTO);

	CHECK(p1.co.X == 3);
	CHECK(p1.co.Y == 9);
	CHECK(p1.interpolation == openshot::InterpolationType::BEZIER);
	CHECK(p1.handle_type == openshot::HandleType::AUTO);
}

TEST_CASE( "Coordinate ctor, BEZIER+MANUAL", "[libopenshot][point]" )
{
	// Create a point with a coordinate and interpolation
	openshot::Coordinate c1(3,9);
	openshot::Point p1(c1,
		openshot::InterpolationType::BEZIER,
		openshot::HandleType::MANUAL);

	CHECK(p1.co.X == 3);
	CHECK(p1.co.Y == 9);
	CHECK(p1.interpolation == openshot::InterpolationType::BEZIER);
	CHECK(p1.handle_type == openshot::HandleType::MANUAL);
}

TEST_CASE( "Json", "[libopenshot][point]" )
{
	openshot::Point p1;
	openshot::Point p2(1, 0);
	auto json1 = p1.Json();
	auto json2 = p2.JsonValue();
	auto json_string2 = json2.toStyledString();
	CHECK(json_string2 == json1);
}

TEST_CASE( "SetJson", "[libopenshot][point]" )
{
	openshot::Point p1;
	std::stringstream json_stream;

	// A string that's not JSON should cause an exception
	CHECK_THROWS_AS(p1.SetJson("}{"), openshot::InvalidJSON);

	// Build a valid JSON string for Point settings
	json_stream << R"json(
		{
			"co": { "X": 1.0, "Y": 0.0 },
			"handle_left": { "X": 2.0, "Y": 3.0 },
			"handle_right": { "X": 4.0, "Y": -2.0 },
			"handle_type": )json";
	json_stream << static_cast<float>(openshot::HandleType::MANUAL) << ",";
	json_stream << R"json(
			"interpolation": )json";
	json_stream << static_cast<float>(openshot::InterpolationType::CONSTANT);
	json_stream << R"json(
		}
		)json";

	p1.SetJson(json_stream.str());
	CHECK(p1.handle_left.X == 2.0);
	CHECK(p1.handle_left.Y == 3.0);
	CHECK(p1.handle_right.X == 4.0);
	CHECK(p1.handle_right.Y == -2.0);
	CHECK(p1.handle_type == openshot::HandleType::MANUAL);
	CHECK(p1.interpolation == openshot::InterpolationType::CONSTANT);
}


TEST_CASE( "Operator ostream", "[libopenshot][point]" )
{
	openshot::Coordinate c1(10, 5);

	std::stringstream output1;
	openshot::Point p1(c1, openshot::InterpolationType::LINEAR);
	output1 << p1;
	CHECK(output1.str() == "co(10, 5) LINEAR");

	std::stringstream output2;
	openshot::Point p2(c1, openshot::InterpolationType::CONSTANT);
	output2 << p2;
	CHECK(output2.str() == "co(10, 5) CONSTANT");

	std::stringstream output3;
	openshot::Point p3(c1, openshot::InterpolationType::BEZIER);
	output3 << p3;
	CHECK(output3.str() == "co(10, 5) BEZIER[L(0.5, 1),R(0.5, 0)]");
}
