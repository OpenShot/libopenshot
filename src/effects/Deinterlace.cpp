/**
 * @file
 * @brief Source file for De-interlace class
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

#include "../../include/effects/Deinterlace.h"

using namespace openshot;

/// Blank constructor, useful when using Json to load the effect properties
Deinterlace::Deinterlace() : isOdd(true) {

}

// Default constructor
Deinterlace::Deinterlace(bool UseOddLines) : isOdd(UseOddLines)
{
	/// Initialize the values of the EffectInfo struct.
	InitEffectInfo();

	/// Set the effect info
	info.name = "Deinterlace";
	info.description = "Remove interlacing from a video (i.e. even or odd horizontal lines)";
	info.has_audio = false;
	info.has_video = true;
}

// This method is required for all derived classes of EffectBase, and returns a
// modified openshot::Frame object
tr1::shared_ptr<Frame> Deinterlace::GetFrame(tr1::shared_ptr<Frame> frame, int frame_number)
{
	// Calculate the new size (used to shrink and expand the image, to remove interlacing)
	Magick::Geometry original_size = frame->GetImage()->size();
	Magick::Geometry frame_size = frame->GetImage()->size();
	frame_size.aspect(false); // allow the image to be re-sized to an invalid aspect ratio
	frame_size.height(frame_size.height() / 2.0); // height set to 50% of original height

	if (isOdd)
		// Roll the image by 1 pixel, to use the ODD horizontal lines (instead of the even ones)
		frame->GetImage()->roll(0,1);

	// Resample the image to 50% height (to remove every other line)
	frame->GetImage()->sample(frame_size);

	// Resize image back to original height
	frame->GetImage()->resize(original_size);

	// return the modified frame
	return frame;
}

// Generate JSON string of this object
string Deinterlace::Json() {

	// Return formatted string
	return JsonValue().toStyledString();
}

// Generate Json::JsonValue for this object
Json::Value Deinterlace::JsonValue() {

	// Create root json object
	Json::Value root = EffectBase::JsonValue(); // get parent properties
	root["type"] = "Deinterlace";
	root["isOdd"] = isOdd;

	// return JsonValue
	return root;
}

// Load JSON string into this object
void Deinterlace::SetJson(string value) throw(InvalidJSON) {

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
void Deinterlace::SetJsonValue(Json::Value root) {

	// Set parent data
	EffectBase::SetJsonValue(root);

	// Set data from Json (if key is found)
	if (!root["isOdd"].isNull())
		isOdd = root["isOdd"].asBool();
}

// Get all properties for a specific frame
string Deinterlace::PropertiesJSON(int requested_frame) {

	// Requested Point
	Point requested_point(requested_frame, requested_frame);

	// Generate JSON properties list
	Json::Value root;
	root["id"] = add_property_json("ID", 0.0, "string", Id(), false, 0, -1, -1, CONSTANT, -1, true);
	root["position"] = add_property_json("Position", Position(), "float", "", false, 0, 0, 1000 * 60 * 30, CONSTANT, -1, false);
	root["layer"] = add_property_json("Layer", Layer(), "int", "", false, 0, 0, 1000, CONSTANT, -1, false);
	root["start"] = add_property_json("Start", Start(), "float", "", false, 0, 0, 1000 * 60 * 30, CONSTANT, -1, false);
	root["end"] = add_property_json("End", End(), "float", "", false, 0, 0, 1000 * 60 * 30, CONSTANT, -1, false);
	root["duration"] = add_property_json("Duration", Duration(), "float", "", false, 0, 0, 1000 * 60 * 30, CONSTANT, -1, true);
	root["isOdd"] = add_property_json("Is Odd Frame", isOdd, "bool", "", false, 0, 0, 1, CONSTANT, -1, true);

	// Keep track of settings string
	stringstream properties;
	properties << 0.0f << Position() << Layer() << Start() << End() << Duration() << isOdd;

	// Add Hash of All property values
	root["hash"] = add_property_json("hash", 0.0, "string", properties.str(), false, 0, 0, 1, CONSTANT, -1, true);

	// Return formatted string
	return root.toStyledString();
}
