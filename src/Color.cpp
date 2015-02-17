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

// Get the HEX value of a color at a specific frame
string Color::GetColorHex(int frame_number) {

	int r = red.GetInt(frame_number);
	int g = green.GetInt(frame_number);
	int b = blue.GetInt(frame_number);

	return QColor( r,g,b ).name().toStdString();
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
}
