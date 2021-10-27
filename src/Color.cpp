/**
 * @file
 * @brief Source file for EffectBase class
 * @author Jonathan Thomas <jonathan@openshot.org>
 *
 * @ref License
 */

// Copyright (c) 2008-2019 OpenShot Studios, LLC
//
// SPDX-License-Identifier: LGPL-3.0-or-later

#include <cmath>

#include "Color.h"
#include "Exceptions.h"

using namespace openshot;

// Constructor which takes R,G,B,A
Color::Color(unsigned char Red, unsigned char Green, unsigned char Blue, unsigned char Alpha) :
    red(static_cast<double>(Red)),
    green(static_cast<double>(Green)),
    blue(static_cast<double>(Blue)),
    alpha(static_cast<double>(Alpha)) { }

// Constructor which takes 4 existing Keyframe curves
Color::Color(Keyframe Red, Keyframe Green, Keyframe Blue, Keyframe Alpha) :
    red(Red), green(Green), blue(Blue), alpha(Alpha) { }

// Constructor which takes a QColor
Color::Color(QColor qcolor) :
    red(qcolor.red()),
    green(qcolor.green()),
    blue(qcolor.blue()),
    alpha(qcolor.alpha()) { }


// Constructor which takes a HEX color code
Color::Color(std::string color_hex)
    : Color::Color(QString::fromStdString(color_hex)) {}

Color::Color(const char* color_hex)
    : Color::Color(QString(color_hex)) {}

// Get the HEX value of a color at a specific frame
std::string Color::GetColorHex(int64_t frame_number) {

	int r = red.GetInt(frame_number);
	int g = green.GetInt(frame_number);
	int b = blue.GetInt(frame_number);
	int a = alpha.GetInt(frame_number);

	return QColor( r,g,b,a ).name().toStdString();
}

// Get RGBA values for a specific frame as an integer vector
std::vector<int> Color::GetColorRGBA(int64_t frame_number) {
	std::vector<int> rgba;
	rgba.push_back(red.GetInt(frame_number));
	rgba.push_back(green.GetInt(frame_number));
	rgba.push_back(blue.GetInt(frame_number));
	rgba.push_back(alpha.GetInt(frame_number));

	return rgba;
}

// Get the distance between 2 RGB pairs (alpha is ignored)
long Color::GetDistance(long R1, long G1, long B1, long R2, long G2, long B2)
{
	  long rmean = ( R1 + R2 ) / 2;
	  long r = R1 - R2;
	  long g = G1 - G2;
	  long b = B1 - B2;
	  return sqrt((((512+rmean)*r*r)>>8) + 4*g*g + (((767-rmean)*b*b)>>8));
}

// Generate JSON string of this object
std::string Color::Json() const {

	// Return formatted string
	return JsonValue().toStyledString();
}

// Generate Json::Value for this object
Json::Value Color::JsonValue() const {

	// Create root json object
	Json::Value root;
	root["red"] = red.JsonValue();
	root["green"] = green.JsonValue();
	root["blue"] = blue.JsonValue();
	root["alpha"] = alpha.JsonValue();

	// return JsonValue
	return root;
}

// Load JSON string into this object
void Color::SetJson(const std::string value) {

	// Parse JSON string into JSON objects
	try
	{
		const Json::Value root = openshot::stringToJson(value);
		// Set all values that match
		SetJsonValue(root);
	}
	catch (const std::exception& e)
	{
		// Error parsing JSON (or missing keys)
		throw InvalidJSON("JSON is invalid (missing keys or invalid data types)");
	}
}

// Load Json::Value into this object
void Color::SetJsonValue(const Json::Value root) {

	// Set data from Json (if key is found)
	if (!root["red"].isNull())
		red.SetJsonValue(root["red"]);
	if (!root["green"].isNull())
		green.SetJsonValue(root["green"]);
	if (!root["blue"].isNull())
		blue.SetJsonValue(root["blue"]);
	if (!root["alpha"].isNull())
		alpha.SetJsonValue(root["alpha"]);
}
