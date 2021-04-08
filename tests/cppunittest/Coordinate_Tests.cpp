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

#include "UnitTest++.h"
// Prevent name clashes with juce::UnitTest
#define DONT_SET_USING_JUCE_NAMESPACE 1
#include "Coordinate.h"
#include "Exceptions.h"

using namespace openshot;

SUITE(Coordinate)
{

TEST(Default_Constructor)
{
	// Create an empty coordinate
	Coordinate c1;

	CHECK_CLOSE(0.0f, c1.X, 0.00001);
	CHECK_CLOSE(0.0f, c1.Y, 0.00001);
}

TEST(X_Y_Constructor)
{
	// Create an empty coordinate
	Coordinate c1(2,8);

	CHECK_CLOSE(2.0f, c1.X, 0.00001);
	CHECK_CLOSE(8.0f, c1.Y, 0.00001);
}

TEST(Pair_Constructor)
{
	Coordinate c1(std::pair<double,double>(12, 10));
	CHECK_CLOSE(12.0f, c1.X, 0.00001);
	CHECK_CLOSE(10.0f, c1.Y, 0.00001);
}

TEST(Json)
{
	openshot::Coordinate c(100, 200);
	openshot::Coordinate c1;
	c1.X = 100;
	c1.Y = 200;
	// Check that JSON produced is identical
	auto j = c.Json();
	auto j1 = c1.Json();
	CHECK_EQUAL(j, j1);
	// Check Json::Value representation
	auto jv = c.JsonValue();
	auto jv_string = jv.toStyledString();
	CHECK_EQUAL(jv_string, j1);
}

TEST(SetJson) {
	// Construct our input Json representation
	const std::string json_input = R"json(
	{
		"X": 100.0,
		"Y": 50.0
	}
		)json";
	openshot::Coordinate c;
	CHECK_THROW(c.SetJson("}{"), openshot::InvalidJSON);
	// Check that values set via SetJson() are correct
	c.SetJson(json_input);
	CHECK_CLOSE(100.0, c.X, 0.01);
	CHECK_CLOSE(50.0, c.Y, 0.01);
}

}  // SUITE
