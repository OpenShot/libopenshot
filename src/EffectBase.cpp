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

#include "../include/EffectBase.h"

using namespace openshot;

// Initialize the values of the FileInfo struct
void EffectBase::InitEffectInfo()
{
	// Init clip settings
	Position(0.0);
	Layer(0);
	Start(0.0);
	End(0.0);
	Order(0);

	info.has_video = false;
	info.has_audio = false;
	info.name = "";
	info.description = "";
}

// Display file information
void EffectBase::DisplayInfo() {
	cout << fixed << setprecision(2) << boolalpha;
	cout << "----------------------------" << endl;
	cout << "----- Effect Information -----" << endl;
	cout << "----------------------------" << endl;
	cout << "--> Name: " << info.name << endl;
	cout << "--> Description: " << info.description << endl;
	cout << "--> Has Video: " << info.has_video << endl;
	cout << "--> Has Audio: " << info.has_audio << endl;
	cout << "----------------------------" << endl;
}

// Generate JSON string of this object
string EffectBase::Json() {

	// Return formatted string
	return JsonValue().toStyledString();
}

// Generate Json::JsonValue for this object
Json::Value EffectBase::JsonValue() {

	// Create root json object
	Json::Value root = ClipBase::JsonValue(); // get parent properties
	root["name"] = info.name;
	root["class_name"] = info.class_name;
	root["short_name"] = info.short_name;
	root["description"] = info.description;
	root["has_video"] = info.has_video;
	root["has_audio"] = info.has_audio;
	root["order"] = Order();

	// return JsonValue
	return root;
}

// Load JSON string into this object
void EffectBase::SetJson(string value) throw(InvalidJSON) {

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
void EffectBase::SetJsonValue(Json::Value root) {

	// Set parent data
	ClipBase::SetJsonValue(root);

	// Set data from Json (if key is found)
	if (!root["order"].isNull())
		Order(root["order"].asInt());
}

// Generate Json::JsonValue for this object
Json::Value EffectBase::JsonInfo() {

	// Create root json object
	Json::Value root;
	root["name"] = info.name;
	root["class_name"] = info.class_name;
	root["short_name"] = info.short_name;
	root["description"] = info.description;
	root["has_video"] = info.has_video;
	root["has_audio"] = info.has_audio;

	// return JsonValue
	return root;
}