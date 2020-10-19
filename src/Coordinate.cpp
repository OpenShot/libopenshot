/**
 * @file
 * @brief Source file for Coordinate class
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

#include "Coordinate.h"

using namespace std;
using namespace openshot;

// Default constructor for a coordinate, which defaults the X and Y to zero (0,0)
Coordinate::Coordinate() :
	X(0), Y(0) {
}

// Constructor which also allows the user to set the X and Y
Coordinate::Coordinate(double x, double y) :
	X(x), Y(y) {
}


// Generate JSON string of this object
std::string Coordinate::Json() const {

	// Return formatted string
	return JsonValue().toStyledString();
}

// Generate Json::Value for this object
Json::Value Coordinate::JsonValue() const {

	// Create root json object
	Json::Value root;
	root["X"] = X;
	root["Y"] = Y;
	//root["increasing"] = increasing;
	//root["repeated"] = Json::Value(Json::objectValue);
	//root["repeated"]["num"] = repeated.num;
	//root["repeated"]["den"] = repeated.den;
	//root["delta"] = delta;

	// return JsonValue
	return root;
}

// Load JSON string into this object
void Coordinate::SetJson(const std::string value) {

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
void Coordinate::SetJsonValue(const Json::Value root) {

	// Set data from Json (if key is found)
	if (!root["X"].isNull())
		X = root["X"].asDouble();
	if (!root["Y"].isNull())
		Y = root["Y"].asDouble();
}
