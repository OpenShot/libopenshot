/**
 * @file
 * @brief Unit tests for openshot::Coordinate
 * @author Jonathan Thomas <jonathan@openshot.org>
 *
 * @ref License
 */

// Copyright (c) 2008-2019 OpenShot Studios, LLC
//
// SPDX-License-Identifier: LGPL-3.0-or-later

#include "openshot_catch.h"

#include "Coordinate.h"
#include "Exceptions.h"

using namespace openshot;

TEST_CASE( "default constructor", "[libopenshot][coordinate]" )
{
	// Create an empty coordinate
	Coordinate c1;

	CHECK(c1.X == Approx(0.0f).margin(0.00001));
	CHECK(c1.Y == Approx(0.0f).margin(0.00001));
}

TEST_CASE( "XY constructor", "[libopenshot][coordinate]" )
{
	// Create an empty coordinate
	Coordinate c1(2,8);

	CHECK(c1.X == Approx(2.0f).margin(0.00001));
	CHECK(c1.Y == Approx(8.0f).margin(0.00001));
}

TEST_CASE( "std::pair constructor", "[libopenshot][coordinate]" )
{
	Coordinate c1(std::pair<double,double>(12, 10));
	CHECK(c1.X == Approx(12.0f).margin(0.00001));
	CHECK(c1.Y == Approx(10.0f).margin(0.00001));
}

TEST_CASE( "Json", "[libopenshot][coordinate]" )
{
	openshot::Coordinate c(100, 200);
	openshot::Coordinate c1;
	c1.X = 100;
	c1.Y = 200;
	// Check that JSON produced is identical
	auto j = c.Json();
	auto j1 = c1.Json();
	CHECK(j1 == j);
	// Check Json::Value representation
	auto jv = c.JsonValue();
	auto jv_string = jv.toStyledString();
	CHECK(j1 == jv_string);
}

TEST_CASE( "SetJson", "[libopenshot][coordinate]" ) {
	// Construct our input Json representation
	const std::string json_input = R"json(
	{
		"X": 100.0,
		"Y": 50.0
	}
		)json";
	openshot::Coordinate c;
	CHECK_THROWS_AS(c.SetJson("}{"), openshot::InvalidJSON);
	// Check that values set via SetJson() are correct
	c.SetJson(json_input);
	CHECK(c.X == Approx(100.0).margin(0.01));
	CHECK(c.Y == Approx(50.0).margin(0.01));
}
