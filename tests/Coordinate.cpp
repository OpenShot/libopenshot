/**
 * @file
 * @brief Unit tests for openshot::Coordinate
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
