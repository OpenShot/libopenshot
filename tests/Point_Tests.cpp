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

#include "UnitTest++.h"
// Prevent name clashes with juce::UnitTest
#define DONT_SET_USING_JUCE_NAMESPACE 1
#include "Point.h"
#include "Enums.h"
#include "Coordinate.h"
#include "Json.h"

SUITE(POINT) {

TEST(Default_Constructor)
{
	openshot::Point p;

	// Default values
	CHECK_EQUAL(1, p.co.X);
	CHECK_EQUAL(0, p.co.Y);
	CHECK_EQUAL(0.5, p.handle_left.X);
	CHECK_EQUAL(1.0, p.handle_left.Y);
	CHECK_EQUAL(0.5, p.handle_right.X);
	CHECK_EQUAL(0.0, p.handle_right.Y);
	CHECK_EQUAL(openshot::InterpolationType::BEZIER, p.interpolation);
	CHECK_EQUAL(openshot::HandleType::AUTO, p.handle_type);
}
TEST(XY_Constructor)
{
	// Create a point with X and Y values
	openshot::Point p1(2,9);

	CHECK_EQUAL(2, p1.co.X);
	CHECK_EQUAL(9, p1.co.Y);
	CHECK_EQUAL(openshot::InterpolationType::BEZIER, p1.interpolation);
}

TEST(Constructor_With_Coordinate)
{
	// Create a point with a coordinate
	openshot::Coordinate c1(3,7);
	openshot::Point p1(c1);

	CHECK_EQUAL(3, p1.co.X);
	CHECK_EQUAL(7, p1.co.Y);
	CHECK_EQUAL(openshot::InterpolationType::BEZIER, p1.interpolation);
}

TEST(Constructor_With_Coordinate_And_LINEAR_Interpolation)
{
	// Create a point with a coordinate and interpolation
	openshot::Coordinate c1(3,9);
	auto interp = openshot::InterpolationType::LINEAR;
	openshot::Point p1(c1, interp);

	CHECK_EQUAL(3, c1.X);
	CHECK_EQUAL(9, c1.Y);
	CHECK_EQUAL(openshot::InterpolationType::LINEAR, p1.interpolation);
}

TEST(Constructor_With_Coordinate_And_BEZIER_Interpolation)
{
	// Create a point with a coordinate and interpolation
	openshot::Coordinate c1(3,9);
	auto interp = openshot::InterpolationType::BEZIER;
	openshot::Point p1(c1, interp);

	CHECK_EQUAL(3, p1.co.X);
	CHECK_EQUAL(9, p1.co.Y);
	CHECK_EQUAL(openshot::InterpolationType::BEZIER, p1.interpolation);
}

TEST(Constructor_With_Coordinate_And_CONSTANT_Interpolation)
{
	// Create a point with a coordinate and interpolation
	openshot::Coordinate c1(2,8);
	auto interp = openshot::InterpolationType::CONSTANT;
	openshot::Point p1(c1, interp);

	CHECK_EQUAL(2, p1.co.X);
	CHECK_EQUAL(8, p1.co.Y);
	CHECK_EQUAL(openshot::InterpolationType::CONSTANT, p1.interpolation);
}

TEST(Constructor_With_Coordinate_And_BEZIER_And_AUTO_Handle)
{
	// Create a point with a coordinate and interpolation
	openshot::Coordinate c1(3,9);
	openshot::Point p1(c1,
		openshot::InterpolationType::BEZIER,
		openshot::HandleType::AUTO);

	CHECK_EQUAL(3, p1.co.X);
	CHECK_EQUAL(9, p1.co.Y);
	CHECK_EQUAL(openshot::InterpolationType::BEZIER, p1.interpolation);
	CHECK_EQUAL(openshot::HandleType::AUTO, p1.handle_type);
}

TEST(Constructor_With_Coordinate_And_BEZIER_And_MANUAL_Handle)
{
	// Create a point with a coordinate and interpolation
	openshot::Coordinate c1(3,9);
	openshot::Point p1(c1,
		openshot::InterpolationType::BEZIER,
		openshot::HandleType::MANUAL);

	CHECK_EQUAL(3, p1.co.X);
	CHECK_EQUAL(9, p1.co.Y);
	CHECK_EQUAL(openshot::InterpolationType::BEZIER, p1.interpolation);
	CHECK_EQUAL(openshot::HandleType::MANUAL, p1.handle_type);
}

TEST(Json)
{
	openshot::Point p1;
	openshot::Point p2(1, 0);
	auto json1 = p1.Json();
	auto json2 = p2.JsonValue();
	auto json_string2 = json2.toStyledString();
	CHECK_EQUAL(json1, json_string2);
}

TEST(SetJson)
{
	openshot::Point p1;
	std::stringstream json_stream;
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
	CHECK_EQUAL(2.0, p1.handle_left.X);
	CHECK_EQUAL(3.0, p1.handle_left.Y);
	CHECK_EQUAL(4.0, p1.handle_right.X);
	CHECK_EQUAL(-2.0, p1.handle_right.Y);
	CHECK_EQUAL(openshot::HandleType::MANUAL, p1.handle_type);
	CHECK_EQUAL(openshot::InterpolationType::CONSTANT, p1.interpolation);
}

}  // SUITE
