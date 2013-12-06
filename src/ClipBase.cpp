/**
 * @file
 * @brief Source file for EffectBase class
 * @author Jonathan Thomas <jonathan@openshot.org>
 *
 * @section LICENSE
 *
 * Copyright (c) 2008-2013 OpenShot Studios, LLC
 * (http://www.openshotstudios.com). This file is part of
 * OpenShot Library (http://www.openshot.org), an open-source project
 * dedicated to delivering high quality video editing and animation solutions
 * to the world.
 *
 * OpenShot Library is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * OpenShot Library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with OpenShot Library.  If not, see <http://www.gnu.org/licenses/>.
 */

#include "../include/ClipBase.h"

using namespace openshot;

// Generate JSON string of this object
string ClipBase::Json() {

	// Return formatted string
	return JsonValue().toStyledString();
}

// Generate Json::JsonValue for this object
Json::Value ClipBase::JsonValue() {

	// Create root json object
	Json::Value root;
	root["position"] = Position();
	root["layer"] = Layer();
	root["start"] = Start();
	root["end"] = End();

	// return JsonValue
	return root;
}

// Load JSON string into this object
void ClipBase::Json(string value) throw(InvalidJSON) {

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
		Json(root);
	}
	catch (exception e)
	{
		// Error parsing JSON (or missing keys)
		throw InvalidJSON("JSON is invalid (missing keys or invalid data types)", "");
	}
}

// Load Json::JsonValue into this object
void ClipBase::Json(Json::Value root) {

	// Set data from Json (if key is found)
	if (root["position"] != Json::nullValue)
		Position(root["position"].asDouble());
	if (root["layer"] != Json::nullValue)
		Layer(root["layer"].asInt());
	if (root["start"] != Json::nullValue)
		Start(root["start"].asDouble());
	if (root["end"] != Json::nullValue)
		End(root["end"].asDouble());
}
