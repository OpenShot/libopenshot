/**
 * @file
 * @brief Source file for ChromaKey class
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

#include "../include/effects/ChromaKey.h"

using namespace openshot;

// Default constructor, which takes an openshot::Color object and a 'fuzz' factor, which
// is used to determine how similar colored pixels are matched. The higher the fuzz, the
// more colors are matched.
ChromaKey::ChromaKey(Color color, Keyframe fuzz) : color(color), fuzz(fuzz)
{
	/// Initialize the values of the EffectInfo struct.
	InitEffectInfo();

	/// Set the effect info
	info.name = "Chroma Key (Greenscreen)";
	info.description = "Replaces the color (or chroma) of the frame with transparency (i.e. keys out the color).";
	info.has_audio = false;
	info.has_video = true;
}

// This method is required for all derived classes of EffectBase, and returns a
// modified openshot::Frame object
tr1::shared_ptr<Frame> ChromaKey::GetFrame(tr1::shared_ptr<Frame> frame, int frame_number)
{
	// Make this range of colors transparent
	frame->GetImage()->colorFuzz(fuzz.GetValue(frame_number) * 65535 / 100.0);
	frame->GetImage()->transparent(Magick::Color(color.red.GetInt(frame_number), color.green.GetInt(frame_number), color.blue.GetInt(frame_number)));

	// return the modified frame
	return frame;
}

// Generate JSON string of this object
string ChromaKey::Json() {

	// Return formatted string
	return JsonValue().toStyledString();
}

// Generate Json::JsonValue for this object
Json::Value ChromaKey::JsonValue() {

	// Create root json object
	Json::Value root = EffectBase::JsonValue(); // get parent properties
	root["color"] = color.JsonValue();
	root["fuzz"] = fuzz.JsonValue();

	// return JsonValue
	return root;
}

// Load JSON string into this object
void ChromaKey::Json(string value) throw(InvalidJSON) {

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
void ChromaKey::Json(Json::Value root) {

	// Set parent data
	EffectBase::Json(root);

	// Set data from Json (if key is found)
	if (root["color"] != Json::nullValue)
		color.Json(root["color"]);
	if (root["fuzz"] != Json::nullValue)
		fuzz.Json(root["fuzz"]);
}
