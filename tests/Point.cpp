/**
 * @file
 * @brief Unit tests for openshot::Point
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

#include "Point.h"
#include "Enums.h"
#include "Exceptions.h"
#include "Coordinate.h"
#include "Json.h"

TEST_CASE( "Default_Constructor", "[libopenshot][point]" )
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
TEST_CASE( "XY_Constructor", "[libopenshot][point]" )
{
	// Create a point with X and Y values
	openshot::Point p1(2,9);

	CHECK(p1.co.X == 2);
	CHECK(p1.co.Y == 9);
	CHECK(p1.interpolation == openshot::InterpolationType::BEZIER);
}

TEST_CASE( "Pair_Constructor", "[libopenshot][point]" )
{
	// Create a point from a std::pair
	std::pair<double, double> coordinates(22, 5);
	openshot::Point p1(coordinates);

	CHECK(p1.co.X == Approx(22.0f).margin(0.00001));
	CHECK(p1.co.Y == Approx(5.0f).margin(0.00001));
}

TEST_CASE( "Constructor_With_Coordinate", "[libopenshot][point]" )
{
	// Create a point with a coordinate
	openshot::Coordinate c1(3,7);
	openshot::Point p1(c1);

	CHECK(p1.co.X == Approx(3.0f).margin(0.00001));
	CHECK(p1.co.Y == Approx(7.0f).margin(0.00001));
	CHECK(p1.interpolation == openshot::InterpolationType::BEZIER);
}

TEST_CASE( "Constructor_With_Coordinate_And_LINEAR_Interpolation", "[libopenshot][point]" )
{
	// Create a point with a coordinate and interpolation
	openshot::Coordinate c1(3,9);
	auto interp = openshot::InterpolationType::LINEAR;
	openshot::Point p1(c1, interp);

	CHECK(c1.X == 3);
	CHECK(c1.Y == 9);
	CHECK(p1.interpolation == openshot::InterpolationType::LINEAR);
}

TEST_CASE( "Constructor_With_Coordinate_And_BEZIER_Interpolation", "[libopenshot][point]" )
{
	// Create a point with a coordinate and interpolation
	openshot::Coordinate c1(3,9);
	auto interp = openshot::InterpolationType::BEZIER;
	openshot::Point p1(c1, interp);

	CHECK(p1.co.X == 3);
	CHECK(p1.co.Y == 9);
	CHECK(p1.interpolation == openshot::InterpolationType::BEZIER);
}

TEST_CASE( "Constructor_With_Coordinate_And_CONSTANT_Interpolation", "[libopenshot][point]" )
{
	// Create a point with a coordinate and interpolation
	openshot::Coordinate c1(2,8);
	auto interp = openshot::InterpolationType::CONSTANT;
	openshot::Point p1(c1, interp);

	CHECK(p1.co.X == 2);
	CHECK(p1.co.Y == 8);
	CHECK(p1.interpolation == openshot::InterpolationType::CONSTANT);
}

TEST_CASE( "Constructor_With_Coordinate_And_BEZIER_And_AUTO_Handle", "[libopenshot][point]" )
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

TEST_CASE( "Constructor_With_Coordinate_And_BEZIER_And_MANUAL_Handle", "[libopenshot][point]" )
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
