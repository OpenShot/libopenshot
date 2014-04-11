/**
 * @file
 * @brief Source file for Negate class
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

#include "../../include/effects/Negate.h"

using namespace openshot;

// Default constructor
Negate::Negate()
{
	/// Initialize the values of the EffectInfo struct.
	InitEffectInfo();

	/// Set the effect info
	info.name = "Negative";
	info.description = "Negates the colors, producing a negative of the image.";
	info.has_audio = false;
	info.has_video = true;
}

// This method is required for all derived classes of EffectBase, and returns a
// modified openshot::Frame object
tr1::shared_ptr<Frame> Negate::GetFrame(tr1::shared_ptr<Frame> frame, int frame_number)
{
	// Make this range of colors transparent
	frame->GetImage()->negate();

	// return the modified frame
	return frame;
}

// Generate JSON string of this object
string Negate::Json() {

	// Return formatted string
	return JsonValue().toStyledString();
}

// Generate Json::JsonValue for this object
Json::Value Negate::JsonValue() {

	// Create root json object
	Json::Value root = EffectBase::JsonValue(); // get parent properties
	root["type"] = "Negate";

	// return JsonValue
	return root;
}

// Load JSON string into this object
void Negate::SetJson(string value) throw(InvalidJSON) {

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
void Negate::SetJsonValue(Json::Value root) {

	// Set parent data
	EffectBase::SetJsonValue(root);

}
