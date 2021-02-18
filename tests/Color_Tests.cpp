/**
 * @file
 * @brief Unit tests for openshot::Color
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

#include <string>
#include <vector>

#include "UnitTest++.h"
// Prevent name clashes with juce::UnitTest
#define DONT_SET_USING_JUCE_NAMESPACE 1
#include "Color.h"
#include "Exceptions.h"
#include "KeyFrame.h"
#include "Json.h"

SUITE(Color) {

TEST(Default_Constructor)
{
	// Create an empty color
	openshot::Color c1;

	CHECK_CLOSE(0.0f, c1.red.GetValue(0), 0.00001);
	CHECK_CLOSE(0.0f, c1.green.GetValue(0), 0.00001);
	CHECK_CLOSE(0.0f, c1.blue.GetValue(0), 0.00001);
}

TEST(Keyframe_constructor)
{
	std::vector<openshot::Keyframe> kfs{0, 0, 0, 0};
	int64_t i(0);
	for (auto& kf : kfs) {
		kf.AddPoint(100, ++i * 20);
	}
	auto c = openshot::Color(kfs[0], kfs[1], kfs[2], kfs[3]);

	CHECK_CLOSE(20, c.red.GetLong(100), 0.01);
	CHECK_CLOSE(40, c.green.GetLong(100), 0.01);
	CHECK_CLOSE(60, c.blue.GetLong(100), 0.01);
	CHECK_CLOSE(80, c.alpha.GetLong(100), 0.01);
}

TEST(Animate_Colors)
{
	// Create an empty color
	openshot::Color c1;

	// Set starting color (on frame 0)
	c1.red.AddPoint(1, 0);
	c1.green.AddPoint(1, 120);
	c1.blue.AddPoint(1, 255);

	// Set ending color (on frame 1000)
	c1.red.AddPoint(1000, 0);
	c1.green.AddPoint(1000, 255);
	c1.blue.AddPoint(1000, 65);

	// Check the color at frame 500
	CHECK_CLOSE(0, c1.red.GetLong(500), 0.01);
	CHECK_CLOSE(187, c1.green.GetLong(500), 0.01);
	CHECK_CLOSE(160, c1.blue.GetLong(500), 0.01);
}

TEST(HEX_Value)
{
	// Color
	openshot::Color c;
	c.red = openshot::Keyframe(0);
	c.red.AddPoint(100, 255);
	c.green = openshot::Keyframe(0);
	c.green.AddPoint(100, 255);
	c.blue = openshot::Keyframe(0);
	c.blue.AddPoint(100, 255);

	CHECK_EQUAL("#000000", c.GetColorHex(1));
	CHECK_EQUAL("#7d7d7d", c.GetColorHex(50));
	CHECK_EQUAL("#ffffff", c.GetColorHex(100));

}

TEST(HEX_Constructor)
{
	// Color
	openshot::Color c("#4586db");
	c.red.AddPoint(100, 255);
	c.green.AddPoint(100, 255);
	c.blue.AddPoint(100, 255);

	CHECK_EQUAL("#4586db", c.GetColorHex(1));
	CHECK_EQUAL("#a0c1ed", c.GetColorHex(50));
	CHECK_EQUAL("#ffffff", c.GetColorHex(100));
}

TEST(Distance)
{
	// Color
	openshot::Color c1("#040a0c");
	openshot::Color c2("#0c0c04");
	openshot::Color c3("#000000");
	openshot::Color c4("#ffffff");

	CHECK_CLOSE(19.0f, openshot::Color::GetDistance(c1.red.GetInt(1), c1.blue.GetInt(1), c1.green.GetInt(1), c2.red.GetInt(1), c2.blue.GetInt(1), c2.green.GetInt(1)), 0.001);
	CHECK_CLOSE(764.0f, openshot::Color::GetDistance(c3.red.GetInt(1), c3.blue.GetInt(1), c3.green.GetInt(1), c4.red.GetInt(1), c4.blue.GetInt(1), c4.green.GetInt(1)), 0.001);
}

TEST(RGBA_Constructor)
{
	// Color
	openshot::Color c(69, 134, 219, 255);
	c.red.AddPoint(100, 255);
	c.green.AddPoint(100, 255);
	c.blue.AddPoint(100, 255);

	CHECK_EQUAL("#4586db", c.GetColorHex(1));
	CHECK_EQUAL("#a0c1ed", c.GetColorHex(50));
	CHECK_EQUAL("#ffffff", c.GetColorHex(100));

	// Color with alpha
	openshot::Color c1(69, 134, 219, 128);
	CHECK_EQUAL("#4586db", c1.GetColorHex(1));
	CHECK_EQUAL(128, c1.alpha.GetInt(1));
}

TEST(Json)
{
	openshot::Color c(128, 128, 128, 0);
	openshot::Color c1;
	c1.red.AddPoint(1, 128);
	c1.green.AddPoint(1, 128);
	c1.blue.AddPoint(1, 128);
	c1.alpha.AddPoint(1, 0);
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
	const std::string json_input = R"json(
	{
		"red": { "Points": [ { "co": { "X": 1.0, "Y": 0.0 }, "interpolation": 0 } ] },
		"green": { "Points": [ { "co": { "X": 1.0, "Y": 128.0 }, "interpolation": 0 } ] },
		"blue": { "Points": [ { "co": { "X": 1.0, "Y": 64.0 }, "interpolation": 0 } ] },
		"alpha": { "Points": [ { "co": { "X": 1.0, "Y": 192.0 }, "interpolation": 0 } ] }
	}
		)json";
	openshot::Color c;
	CHECK_THROW(c.SetJson("}{"), openshot::InvalidJSON);
	c.SetJson(json_input);
	CHECK_CLOSE(0, c.red.GetLong(10), 0.01);
	CHECK_CLOSE(128, c.green.GetLong(10), 0.01);
	CHECK_CLOSE(64, c.blue.GetLong(10), 0.01);
	CHECK_CLOSE(192, c.alpha.GetLong(10), 0.01);
}

}  // SUITE
