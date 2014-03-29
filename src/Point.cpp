/**
 * @file
 * @brief Source file for Point class
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
 * and/or modify it under the terms of the GNU Affero General Public License
 * as published by the Free Software Foundation, either version 3 of the
 * License, or (at your option) any later version.
 *
 * OpenShot Library (libopenshot) is distributed in the hope that it will be
 * useful, but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU Affero General Public License for more details.
 *
 * You should have received a copy of the GNU Affero General Public License
 * along with OpenShot Library. If not, see <http://www.gnu.org/licenses/>.
 *
 * Also, if your software can interact with users remotely through a computer
 * network, you should also make sure that it provides a way for users to
 * get its source. For example, if your program is a web application, its
 * interface could display a "Source" link that leads users to an archive
 * of the code. There are many ways you could offer source, and different
 * solutions will be better for different programs; see section 13 for the
 * specific requirements.
 *
 * You should also get your employer (if you work as a programmer) or school,
 * if any, to sign a "copyright disclaimer" for the program, if necessary.
 * For more information on this, and how to apply and follow the GNU AGPL, see
 * <http://www.gnu.org/licenses/>.
 */

#include "../include/Point.h"

using namespace std;
using namespace openshot;

// Default constructor (defaults to 0,0)
Point::Point() : interpolation(BEZIER), handle_type(AUTO)
{
	// set new coorinate
	co = Coordinate(0, 0);

	// set handles
	Initialize_Handles();
}

// Constructor which creates a single coordinate at X=0
Point::Point(float y) :
	interpolation(CONSTANT), handle_type(AUTO) {
	// set new coorinate
	co = Coordinate(0, y);

	// set handles
	Initialize_Handles();
}

Point::Point(float x, float y) :
	interpolation(BEZIER), handle_type(AUTO) {
	// set new coorinate
	co = Coordinate(x, y);

	// set handles
	Initialize_Handles();
}

// Constructor which also creates a Point and sets the X,Y, and interpolation of the Point.
Point::Point(float x, float y, InterpolationType interpolation) :
		handle_type(AUTO), interpolation(interpolation) {
	// set new coorinate
	co = Coordinate(x, y);

	// set handles
	Initialize_Handles();
}

Point::Point(Coordinate co) :
	co(co), interpolation(BEZIER), handle_type(AUTO) {
	// set handles
	Initialize_Handles();
}

Point::Point(Coordinate co, InterpolationType interpolation) :
	co(co), interpolation(interpolation), handle_type(AUTO) {
	// set handles
	Initialize_Handles();
}

Point::Point(Coordinate co, InterpolationType interpolation, HandleType handle_type) :
	co(co), interpolation(interpolation), handle_type(handle_type) {
	// set handles
	Initialize_Handles();
}

void Point::Initialize_Handles(float Offset) {
	// initialize left and right handles
	handle_left = Coordinate(co.X - Offset, co.Y);
	handle_right = Coordinate(co.X + Offset, co.Y);
}

// Generate JSON string of this object
string Point::Json() {

	// Return formatted string
	return JsonValue().toStyledString();
}

// Generate Json::JsonValue for this object
Json::Value Point::JsonValue() {

	// Create root json object
	Json::Value root;
	root["co"] = co.JsonValue();
	if (interpolation == BEZIER) {
		root["handle_left"] = handle_left.JsonValue();
		root["handle_right"] = handle_right.JsonValue();
		root["handle_type"] = handle_type;
	}
	root["interpolation"] = interpolation;

	// return JsonValue
	return root;
}

// Load JSON string into this object
void Point::SetJson(string value) throw(InvalidJSON) {

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
void Point::SetJsonValue(Json::Value root) {

	if (!root["co"].isNull())
		co.SetJsonValue(root["co"]); // update coordinate
	if (!root["handle_left"].isNull())
		handle_left.SetJsonValue(root["handle_left"]); // update coordinate
	if (!root["handle_right"].isNull())
		handle_right.SetJsonValue(root["handle_right"]); // update coordinate
	if (!root["interpolation"].isNull())
		interpolation = (InterpolationType) root["interpolation"].asInt();
	if (!root["handle_type"].isNull())
		handle_type = (HandleType) root["handle_type"].asInt();

}
