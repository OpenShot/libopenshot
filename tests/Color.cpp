/**
 * @file
 * @brief Unit tests for openshot::Color
 * @author Jonathan Thomas <jonathan@openshot.org>
 *
 * @ref License
 */

// Copyright (c) 2008-2019 OpenShot Studios, LLC
//
// SPDX-License-Identifier: LGPL-3.0-or-later

#include <string>
#include <vector>

#include "openshot_catch.h"

#include <QColor>

#include "Color.h"
#include "Exceptions.h"
#include "KeyFrame.h"
#include "Json.h"

TEST_CASE( "default constructor", "[libopenshot][color]" )
{
	// Create an empty color
	openshot::Color c1;

	CHECK(c1.red.GetValue(0) == Approx(0.0f).margin(0.00001));
	CHECK(c1.green.GetValue(0) == Approx(0.0f).margin(0.00001));
	CHECK(c1.blue.GetValue(0) == Approx(0.0f).margin(0.00001));
}

TEST_CASE( "Keyframe constructor", "[libopenshot][color]" )
{
	std::vector<openshot::Keyframe> kfs{0, 0, 0, 0};
	int64_t i(0);
	for (auto& kf : kfs) {
		kf.AddPoint(100, ++i * 20);
	}
	auto c = openshot::Color(kfs[0], kfs[1], kfs[2], kfs[3]);

	CHECK(c.red.GetLong(100) == Approx(20).margin(0.01));
	CHECK(c.green.GetLong(100) == Approx(40).margin(0.01));
	CHECK(c.blue.GetLong(100) == Approx(60).margin(0.01));
	CHECK(c.alpha.GetLong(100) == Approx(80).margin(0.01));
}

TEST_CASE( "Animate_Colors", "[libopenshot][color]" )
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
	CHECK(c1.red.GetLong(500) == Approx(0).margin(0.01));
	CHECK(c1.green.GetLong(500) == Approx(187).margin(0.01));
	CHECK(c1.blue.GetLong(500) == Approx(160).margin(0.01));
}

TEST_CASE( "HEX_Value", "[libopenshot][color]" )
{
	// Color
	openshot::Color c;
	c.red = openshot::Keyframe(0);
	c.red.AddPoint(100, 255);
	c.green = openshot::Keyframe(0);
	c.green.AddPoint(100, 255);
	c.blue = openshot::Keyframe(0);
	c.blue.AddPoint(100, 255);

	CHECK(c.GetColorHex(1) == "#000000");
	CHECK(c.GetColorHex(50) == "#7d7d7d");
	CHECK(c.GetColorHex(100) == "#ffffff");

}

TEST_CASE( "QColor ctor", "[libopenshot][color]" )
{
    QColor qc(Qt::red);
    openshot::Color c(qc);

    CHECK(c.red.GetLong(1) == Approx(255.0).margin(0.0001));
    CHECK(c.green.GetLong(1) == Approx(0.0).margin(0.0001));
    CHECK(c.blue.GetLong(1) == Approx(0.0).margin(0.0001));
    CHECK(c.alpha.GetLong(1) == Approx(255.0).margin(0.0001));
}

TEST_CASE( "std::string construction", "[libopenshot][color]" )
{
	// Color
	openshot::Color c("#4586db");
	c.red.AddPoint(100, 255);
	c.green.AddPoint(100, 255);
	c.blue.AddPoint(100, 255);

	CHECK(c.GetColorHex(1) == "#4586db");
	CHECK(c.GetColorHex(50) == "#a0c1ed");
	CHECK(c.GetColorHex(100) == "#ffffff");
}

TEST_CASE( "Distance", "[libopenshot][color]" )
{
	// Color
	openshot::Color c1("#040a0c");
	openshot::Color c2("#0c0c04");
	openshot::Color c3("#000000");
	openshot::Color c4("#ffffff");

	CHECK(
		openshot::Color::GetDistance(
			c1.red.GetInt(1), c1.blue.GetInt(1), c1.green.GetInt(1),
			c2.red.GetInt(1), c2.blue.GetInt(1), c2.green.GetInt(1)
		) == Approx(19.0f).margin(0.001));
	CHECK(
		openshot::Color::GetDistance(
			c3.red.GetInt(1), c3.blue.GetInt(1), c3.green.GetInt(1),
			c4.red.GetInt(1), c4.blue.GetInt(1), c4.green.GetInt(1)
		) == Approx(764.0f).margin(0.001));
}

TEST_CASE( "RGBA_Constructor", "[libopenshot][color]" )
{
	// Color
	openshot::Color c(69, 134, 219, 255);
	c.red.AddPoint(100, 255);
	c.green.AddPoint(100, 255);
	c.blue.AddPoint(100, 255);

	CHECK(c.GetColorHex(1) == "#4586db");
	CHECK(c.GetColorHex(50) == "#a0c1ed");
	CHECK(c.GetColorHex(100) == "#ffffff");

	// Color with alpha
	openshot::Color c1(69, 134, 219, 128);
	CHECK(c1.GetColorHex(1) == "#4586db");
	CHECK(c1.alpha.GetInt(1) == 128);
}

TEST_CASE( "Json", "[libopenshot][color]" )
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
	CHECK(j1 == j);
	// Check Json::Value representation
	auto jv = c.JsonValue();
	auto jv_string = jv.toStyledString();
	CHECK(j1 == jv_string);
}

TEST_CASE( "SetJson", "[libopenshot][color]" ) {
	const std::string json_input = R"json(
	{
		"red": { "Points": [ { "co": { "X": 1.0, "Y": 0.0 }, "interpolation": 0 } ] },
		"green": { "Points": [ { "co": { "X": 1.0, "Y": 128.0 }, "interpolation": 0 } ] },
		"blue": { "Points": [ { "co": { "X": 1.0, "Y": 64.0 }, "interpolation": 0 } ] },
		"alpha": { "Points": [ { "co": { "X": 1.0, "Y": 192.0 }, "interpolation": 0 } ] }
	}
		)json";
	openshot::Color c;
	CHECK_THROWS_AS(c.SetJson("}{"), openshot::InvalidJSON);
	c.SetJson(json_input);
	CHECK(c.red.GetLong(10) == Approx(0).margin(0.01));
	CHECK(c.green.GetLong(10) == Approx(128).margin(0.01));
	CHECK(c.blue.GetLong(10) == Approx(64).margin(0.01));
	CHECK(c.alpha.GetLong(10) == Approx(192).margin(0.01));
}
