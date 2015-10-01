/**
 * @file
 * @brief Source file for EffectBase class
 * @author Jonathan Thomas <jonathan@openshot.org>
 *
 * @section LICENSE
 *
 * Copyright (c) 2008-2014 OpenShot Studios, LLC
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

#include "../include/Color.h"

using namespace openshot;

// Constructor which takes R,G,B,A
Color::Color(unsigned char Red, unsigned char Green, unsigned char Blue, unsigned char Alpha)
{
	// Set initial points
	red.AddPoint(1, (float)Red);
	green.AddPoint(1, (float)Green);
	blue.AddPoint(1, (float)Blue);
	alpha.AddPoint(1, (float)Alpha);
}

// Constructor which takes 4 existing Keyframe curves
Color::Color(Keyframe Red, Keyframe Green, Keyframe Blue, Keyframe Alpha)
{
	// Assign existing keyframes
	red = Red;
	green = Green;
	blue = Blue;
	alpha = Alpha;
}

// Constructor which takes a HEX color code
Color::Color(string color_hex)
{
	// Create a QColor from hex
	QColor color(QString::fromStdString(color_hex));
	red.AddPoint(1, color.red());
	green.AddPoint(1, color.green());
	blue.AddPoint(1, color.blue());
	alpha.AddPoint(1, color.alpha());
}

// Get the HEX value of a color at a specific frame
string Color::GetColorHex(long int frame_number) {

	int r = red.GetInt(frame_number);
	int g = green.GetInt(frame_number);
	int b = blue.GetInt(frame_number);
	int a = alpha.GetInt(frame_number);

	return QColor( r,g,b,a ).name().toStdString();
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
string Color::Json() {

	// Return formatted string
	return JsonValue().toStyledString();
}

// Generate Json::JsonValue for this object
Json::Value Color::JsonValue() {

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
void Color::SetJson(string value) throw(InvalidJSON) {

	// Parse JSON string into JSON objects
	Json::Value root;
	Json::Reader reader;
	bool success = reader.parse( value, root );
	if (!success)
		// Raise exception
		throw InvalidJSON("JSON could not be parsed (or is invalid)", "");

	try
	{
		// Set all values that match
		SetJsonValue(root);
	}
	catch (exception e)
	{
		// Error parsing JSON (or missing keys)
		throw InvalidJSON("JSON is invalid (missing keys or invalid data types)", "");
	}
}

// Load Json::JsonValue into this object
void Color::SetJsonValue(Json::Value root) {

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
