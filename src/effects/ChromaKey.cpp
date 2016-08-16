/**
 * @file
 * @brief Source file for ChromaKey class
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

#include "../../include/effects/ChromaKey.h"

using namespace openshot;

/// Blank constructor, useful when using Json to load the effect properties
ChromaKey::ChromaKey() : fuzz(5.0) {
	// Init default color
	color = Color();

	// Init effect properties
	init_effect_details();
}

// Default constructor, which takes an openshot::Color object and a 'fuzz' factor, which
// is used to determine how similar colored pixels are matched. The higher the fuzz, the
// more colors are matched.
ChromaKey::ChromaKey(Color color, Keyframe fuzz) : color(color), fuzz(fuzz)
{
	// Init effect properties
	init_effect_details();
}

// Init effect settings
void ChromaKey::init_effect_details()
{
	/// Initialize the values of the EffectInfo struct.
	InitEffectInfo();

	/// Set the effect info
	info.class_name = "ChromaKey";
	info.name = "Chroma Key (Greenscreen)";
	info.description = "Replaces the color (or chroma) of the frame with transparency (i.e. keys out the color).";
	info.has_audio = false;
	info.has_video = true;
}

// This method is required for all derived classes of EffectBase, and returns a
// modified openshot::Frame object
tr1::shared_ptr<Frame> ChromaKey::GetFrame(tr1::shared_ptr<Frame> frame, long int frame_number)
{
	// Determine the current HSL (Hue, Saturation, Lightness) for the Chrome
	int threshold = fuzz.GetInt(frame_number);
	long mask_R = color.red.GetInt(frame_number);
	long mask_G = color.green.GetInt(frame_number);
	long mask_B = color.blue.GetInt(frame_number);

	// Get source image's pixels
	tr1::shared_ptr<QImage> image = frame->GetImage();
	unsigned char *pixels = (unsigned char *) image->bits();

	// Loop through pixels
	for (int pixel = 0, byte_index=0; pixel < image->width() * image->height(); pixel++, byte_index+=4)
	{
		// Get the RGB values from the pixel
		unsigned char R = pixels[byte_index];
		unsigned char G = pixels[byte_index + 1];
		unsigned char B = pixels[byte_index + 2];
		unsigned char A = pixels[byte_index + 3];

		// Get distance between mask color and pixel color
		long distance = Color::GetDistance((long)R, (long)G, (long)B, mask_R, mask_G, mask_B);

		// Alpha out the pixel (if color similar)
		if (distance <= threshold)
			// MATCHED - Make pixel transparent
			pixels[byte_index + 3] = 0;
	}

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
	root["type"] = info.class_name;
	root["color"] = color.JsonValue();
	root["fuzz"] = fuzz.JsonValue();

	// return JsonValue
	return root;
}

// Load JSON string into this object
void ChromaKey::SetJson(string value) throw(InvalidJSON) {

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
void ChromaKey::SetJsonValue(Json::Value root) {

	// Set parent data
	EffectBase::SetJsonValue(root);

	// Set data from Json (if key is found)
	if (!root["color"].isNull())
		color.SetJsonValue(root["color"]);
	if (!root["fuzz"].isNull())
		fuzz.SetJsonValue(root["fuzz"]);
}

// Get all properties for a specific frame
string ChromaKey::PropertiesJSON(long int requested_frame) {

	// Requested Point
	Point requested_point(requested_frame, requested_frame);

	// Generate JSON properties list
	Json::Value root;
	root["id"] = add_property_json("ID", 0.0, "string", Id(), false, 0, -1, -1, CONSTANT, -1, true);
	root["position"] = add_property_json("Position", Position(), "float", "", false, 0, 0, 30 * 60 * 60 * 48, CONSTANT, -1, false);
	root["layer"] = add_property_json("Layer", Layer(), "int", "", false, 0, 0, 20, CONSTANT, -1, false);
	root["start"] = add_property_json("Start", Start(), "float", "", false, 0, 0, 30 * 60 * 60 * 48, CONSTANT, -1, false);
	root["end"] = add_property_json("End", End(), "float", "", false, 0, 0, 30 * 60 * 60 * 48, CONSTANT, -1, false);
	root["duration"] = add_property_json("Duration", Duration(), "float", "", false, 0, 0, 30 * 60 * 60 * 48, CONSTANT, -1, true);

	// Keyframes
	root["color"] = add_property_json("Key Color", 0.0, "color", "", color.red.Contains(requested_point), color.red.GetCount(), 0, 255, color.red.GetClosestPoint(requested_point).interpolation, color.red.GetClosestPoint(requested_point).co.X, false);
	root["color"]["red"] = add_property_json("Red", color.red.GetValue(requested_frame), "float", "", color.red.Contains(requested_point), color.red.GetCount(), 0, 255, color.red.GetClosestPoint(requested_point).interpolation, color.red.GetClosestPoint(requested_point).co.X, false);
	root["color"]["blue"] = add_property_json("Blue", color.blue.GetValue(requested_frame), "float", "", color.blue.Contains(requested_point), color.blue.GetCount(), 0, 255, color.blue.GetClosestPoint(requested_point).interpolation, color.blue.GetClosestPoint(requested_point).co.X, false);
	root["color"]["green"] = add_property_json("Green", color.green.GetValue(requested_frame), "float", "", color.green.Contains(requested_point), color.green.GetCount(), 0, 255, color.green.GetClosestPoint(requested_point).interpolation, color.green.GetClosestPoint(requested_point).co.X, false);
	root["fuzz"] = add_property_json("Fuzz", fuzz.GetValue(requested_frame), "float", "", fuzz.Contains(requested_point), fuzz.GetCount(), 0, 25, fuzz.GetClosestPoint(requested_point).interpolation, fuzz.GetClosestPoint(requested_point).co.X, false);

	// Return formatted string
	return root.toStyledString();
}
