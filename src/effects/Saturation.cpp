/**
 * @file
 * @brief Source file for Saturation class
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

#include "../../include/effects/Saturation.h"

using namespace openshot;

/// Blank constructor, useful when using Json to load the effect properties
Saturation::Saturation() : saturation(1.0), saturation_R(1.0), saturation_G(1.0), saturation_B(1.0) {
	// Init effect properties
	init_effect_details();
}

// Default constructor
Saturation::Saturation(Keyframe saturation, Keyframe saturation_R, Keyframe saturation_G, Keyframe saturation_B) : saturation(saturation), saturation_R(saturation_R), saturation_G(saturation_G), saturation_B(saturation_B)
{
	// Init effect properties
	init_effect_details();
}

// Init effect settings
void Saturation::init_effect_details()
{
	/// Initialize the values of the EffectInfo struct.
	InitEffectInfo();

	/// Set the effect info
	info.class_name = "Saturation";
	info.name = "Color Saturation";
	info.description = "Adjust the color saturation.";
	info.has_audio = false;
	info.has_video = true;
}

// This method is required for all derived classes of EffectBase, and returns a
// modified openshot::Frame object
std::shared_ptr<Frame> Saturation::GetFrame(std::shared_ptr<Frame> frame, int64_t frame_number)
{
	// Get the frame's image
	std::shared_ptr<QImage> frame_image = frame->GetImage();

	if (!frame_image)
		return frame;

	// Get keyframe values for this frame
	float saturation_value = saturation.GetValue(frame_number);
	float saturation_value_R = saturation_R.GetValue(frame_number);
	float saturation_value_G = saturation_G.GetValue(frame_number);
	float saturation_value_B = saturation_B.GetValue(frame_number);

	// Constants used for color saturation formula
	double pR = .299;
	double pG = .587;
	double pB = .114;

	// Loop through pixels
	unsigned char *pixels = (unsigned char *) frame_image->bits();
	for (int pixel = 0, byte_index=0; pixel < frame_image->width() * frame_image->height(); pixel++, byte_index+=4)
	{
		// Get the RGB values from the pixel
		int R = pixels[byte_index];
		int G = pixels[byte_index + 1];
		int B = pixels[byte_index + 2];
		int A = pixels[byte_index + 3];

		/*
		 * Common saturation adjustment
		 */

		// Calculate the saturation multiplier
		double p = sqrt( (R * R * pR) +
						 (G * G * pG) +
						 (B * B * pB) );

		// Adjust the saturation
		R = p + (R - p) * saturation_value;
		G = p + (G - p) * saturation_value;
		B = p + (B - p) * saturation_value;

		// Constrain the value from 0 to 255
		R = constrain(R);
		G = constrain(G);
		B = constrain(B);

		/*
		 * Color-separated saturation adjustment
		 *
		 * Splitting each of the three subpixels (R, G and B) into three distincs sub-subpixels (R, G and B in turn)
		 * which in their optical sum reproduce the original subpixel's color OR produce white light in the brightness
		 * of the original subpixel (dependening on the color channel's slider value).
		 */

		// Three subpixels producing either R or white with brightness of R
		int Rr = R;
		int Gr = 0;
		int Br = 0;

		// Three subpixels producing either G or white with brightness of G
		int Rg = 0;
		int Gg = G;
		int Bg = 0;

		// Three subpixels producing either B or white with brightness of B
		int Rb = 0;
		int Gb = 0;
		int Bb = B;

		// Compute the brightness ("saturation multiplier") of the replaced subpixels
		// Actually mathematical no-ops mostly, verbosity is kept just for clarification
		const double p_r = sqrt( (Rr * Rr * pR) + (Gr * Gr * pG) + (Br * Br * pB) );
		const double p_g = sqrt( (Rg * Rg * pR) + (Gg * Gg * pG) + (Bg * Bg * pB) );
		const double p_b = sqrt( (Rb * Rb * pR) + (Gb * Gb * pG) + (Bb * Bb * pB) );

		// Adjust the saturation
		Rr = p_r + (Rr - p_r) * saturation_value_R;
		Gr = p_r + (Gr - p_r) * saturation_value_R;
		Br = p_r + (Br - p_r) * saturation_value_R;

		Rg = p_g + (Rg - p_g) * saturation_value_G;
		Gg = p_g + (Gg - p_g) * saturation_value_G;
		Bg = p_g + (Bg - p_g) * saturation_value_G;

		Rb = p_b + (Rb - p_b) * saturation_value_B;
		Gb = p_b + (Gb - p_b) * saturation_value_B;
		Bb = p_b + (Bb - p_b) * saturation_value_B;

		// Recombine brightness of sub-subpixels (Rx, Gx and Bx) into sub-pixels (R, G and B) again
		R = Rr + Rg + Rb;
		G = Gr + Gg + Gb;
		B = Br + Bg + Bb;

		// Constrain the value from 0 to 255
		R = constrain(R);
		G = constrain(G);
		B = constrain(B);

		// Set all pixels to new value
		pixels[byte_index] = R;
		pixels[byte_index + 1] = G;
		pixels[byte_index + 2] = B;
		pixels[byte_index + 3] = A; // leave the alpha value alone
	}

	// return the modified frame
	return frame;
}

// Generate JSON string of this object
std::string Saturation::Json() {

	// Return formatted string
	return JsonValue().toStyledString();
}

// Generate Json::JsonValue for this object
Json::Value Saturation::JsonValue() {

	// Create root json object
	Json::Value root = EffectBase::JsonValue(); // get parent properties
	root["type"] = info.class_name;
	root["saturation"] = saturation.JsonValue();
	root["saturation_R"] = saturation_R.JsonValue();
	root["saturation_G"] = saturation_G.JsonValue();
	root["saturation_B"] = saturation_B.JsonValue();

	// return JsonValue
	return root;
}

// Load JSON string into this object
void Saturation::SetJson(std::string value) {

	// Parse JSON string into JSON objects
	Json::Value root;
	Json::CharReaderBuilder rbuilder;
	Json::CharReader* reader(rbuilder.newCharReader());

	std::string errors;
	bool success = reader->parse( value.c_str(),
                 value.c_str() + value.size(), &root, &errors );
	delete reader;

	if (!success)
		// Raise exception
		throw InvalidJSON("JSON could not be parsed (or is invalid)");

	try
	{
		// Set all values that match
		SetJsonValue(root);
	}
	catch (const std::exception& e)
	{
		// Error parsing JSON (or missing keys)
		throw InvalidJSON("JSON is invalid (missing keys or invalid data types)");
	}
}

// Load Json::JsonValue into this object
void Saturation::SetJsonValue(Json::Value root) {

	// Set parent data
	EffectBase::SetJsonValue(root);

	// Set data from Json (if key is found)
	if (!root["saturation"].isNull())
		saturation.SetJsonValue(root["saturation"]);
	if (!root["saturation_R"].isNull())
		saturation_R.SetJsonValue(root["saturation_R"]);
	if (!root["saturation_G"].isNull())
		saturation_G.SetJsonValue(root["saturation_G"]);
	if (!root["saturation_B"].isNull())
		saturation_B.SetJsonValue(root["saturation_B"]);
}

// Get all properties for a specific frame
std::string Saturation::PropertiesJSON(int64_t requested_frame) {

	// Generate JSON properties list
	Json::Value root;
	root["id"] = add_property_json("ID", 0.0, "string", Id(), NULL, -1, -1, true, requested_frame);
	root["position"] = add_property_json("Position", Position(), "float", "", NULL, 0, 30 * 60 * 60 * 48, false, requested_frame);
	root["layer"] = add_property_json("Track", Layer(), "int", "", NULL, 0, 20, false, requested_frame);
	root["start"] = add_property_json("Start", Start(), "float", "", NULL, 0, 30 * 60 * 60 * 48, false, requested_frame);
	root["end"] = add_property_json("End", End(), "float", "", NULL, 0, 30 * 60 * 60 * 48, false, requested_frame);
	root["duration"] = add_property_json("Duration", Duration(), "float", "", NULL, 0, 30 * 60 * 60 * 48, true, requested_frame);

	// Keyframes
	root["saturation"] = add_property_json("Saturation", saturation.GetValue(requested_frame), "float", "", &saturation, 0.0, 4.0, false, requested_frame);
	root["saturation_R"] = add_property_json("Saturation (Red)", saturation_R.GetValue(requested_frame), "float", "", &saturation_R, 0.0, 4.0, false, requested_frame);
	root["saturation_G"] = add_property_json("Saturation (Green)", saturation_G.GetValue(requested_frame), "float", "", &saturation_G, 0.0, 4.0, false, requested_frame);
	root["saturation_B"] = add_property_json("Saturation (Blue)", saturation_B.GetValue(requested_frame), "float", "", &saturation_B, 0.0, 4.0, false, requested_frame);

	// Return formatted string
	return root.toStyledString();
}
