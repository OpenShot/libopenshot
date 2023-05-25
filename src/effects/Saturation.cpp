/**
 * @file
 * @brief Source file for Saturation class
 * @author Jonathan Thomas <jonathan@openshot.org>
 *
 * @ref License
 */

// Copyright (c) 2008-2019 OpenShot Studios, LLC
//
// SPDX-License-Identifier: LGPL-3.0-or-later

#include "Saturation.h"
#include "Exceptions.h"

using namespace openshot;

/// Blank constructor, useful when using Json to load the effect properties
Saturation::Saturation() : saturation(1.0), saturation_R(1.0), saturation_G(1.0), saturation_B(1.0) {
	// Init effect properties
	init_effect_details();
}

// Default constructor
Saturation::Saturation(Keyframe saturation, Keyframe saturation_R, Keyframe saturation_G, Keyframe saturation_B) :
		saturation(saturation), saturation_R(saturation_R), saturation_G(saturation_G), saturation_B(saturation_B)
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
std::shared_ptr<openshot::Frame> Saturation::GetFrame(std::shared_ptr<openshot::Frame> frame, int64_t frame_number)
{
	// Get the frame's image
	std::shared_ptr<QImage> frame_image = frame->GetImage();

	if (!frame_image)
		return frame;

	int pixel_count = frame_image->width() * frame_image->height();

	// Get keyframe values for this frame
	float saturation_value = saturation.GetValue(frame_number);
	float saturation_value_R = saturation_R.GetValue(frame_number);
	float saturation_value_G = saturation_G.GetValue(frame_number);
	float saturation_value_B = saturation_B.GetValue(frame_number);

	// Constants used for color saturation formula
	const double pR = .299;
	const double pG = .587;
	const double pB = .114;

	// Loop through pixels
	unsigned char *pixels = (unsigned char *) frame_image->bits();

	#pragma omp parallel for shared (pixels)
	for (int pixel = 0; pixel < pixel_count; ++pixel)
	{
		// Calculate alpha % (to be used for removing pre-multiplied alpha value)
		int A = pixels[pixel * 4 + 3];
		float alpha_percent = A / 255.0;

		// Get RGB values, and remove pre-multiplied alpha
		int R = pixels[pixel * 4 + 0] / alpha_percent;
		int G = pixels[pixel * 4 + 1] / alpha_percent;
		int B = pixels[pixel * 4 + 2] / alpha_percent;

		/*
		 * Common saturation adjustment
		 */

		// Calculate the saturation multiplier
		double p = sqrt( (R * R * pR) +
						 (G * G * pG) +
						 (B * B * pB) );

		// Adjust the saturation
		R = constrain(p + (R - p) * saturation_value);
		G = constrain(p + (G - p) * saturation_value);
		B = constrain(p + (B - p) * saturation_value);

		/*
		 * Color-separated saturation adjustment
		 *
		 * Splitting each of the three subpixels (R, G and B) into three distincs sub-subpixels (R, G and B in turn)
		 * which in their optical sum reproduce the original subpixel's color OR produce white light in the brightness
		 * of the original subpixel (dependening on the color channel's slider value).
		 */

		// Compute the brightness ("saturation multiplier") of the replaced subpixels
		// Actually mathematical no-ops mostly, verbosity is kept just for clarification
		const double p_r = sqrt(R * R * pR);
		const double p_g = sqrt(G * G * pG);
		const double p_b = sqrt(B * B * pB);

		// Adjust the saturation
		const int Rr = p_r + (R - p_r) * saturation_value_R;
		const int Gr = p_r + (0 - p_r) * saturation_value_R;
		const int Br = p_r + (0 - p_r) * saturation_value_R;

		const int Rg = p_g + (0 - p_g) * saturation_value_G;
		const int Gg = p_g + (G - p_g) * saturation_value_G;
		const int Bg = p_g + (0 - p_g) * saturation_value_G;

		const int Rb = p_b + (0 - p_b) * saturation_value_B;
		const int Gb = p_b + (0 - p_b) * saturation_value_B;
		const int Bb = p_b + (B - p_b) * saturation_value_B;

		// Recombine brightness of sub-subpixels (Rx, Gx and Bx) into sub-pixels (R, G and B) again
		R = Rr + Rg + Rb;
		G = Gr + Gg + Gb;
		B = Br + Bg + Bb;

		// Constrain the value from 0 to 255
		R = constrain(R);
		G = constrain(G);
		B = constrain(B);

		// Set all pixels to new value
		pixels[pixel * 4 + 0] = R;
		pixels[pixel * 4 + 1] = G;
		pixels[pixel * 4 + 2] = B;

		// Pre-multiply the alpha back into the color channels
		pixels[pixel * 4 + 0] *= alpha_percent;
		pixels[pixel * 4 + 1] *= alpha_percent;
		pixels[pixel * 4 + 2] *= alpha_percent;
	}

	// return the modified frame
	return frame;
}

// Generate JSON string of this object
std::string Saturation::Json() const {

	// Return formatted string
	return JsonValue().toStyledString();
}

// Generate Json::Value for this object
Json::Value Saturation::JsonValue() const {

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
void Saturation::SetJson(const std::string value) {

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
void Saturation::SetJsonValue(const Json::Value root) {

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
std::string Saturation::PropertiesJSON(int64_t requested_frame) const {

	// Generate JSON properties list
	Json::Value root = BasePropertiesJSON(requested_frame);

	// Keyframes
	root["saturation"] = add_property_json("Saturation", saturation.GetValue(requested_frame), "float", "", &saturation, 0.0, 4.0, false, requested_frame);
	root["saturation_R"] = add_property_json("Saturation (Red)", saturation_R.GetValue(requested_frame), "float", "", &saturation_R, 0.0, 4.0, false, requested_frame);
	root["saturation_G"] = add_property_json("Saturation (Green)", saturation_G.GetValue(requested_frame), "float", "", &saturation_G, 0.0, 4.0, false, requested_frame);
	root["saturation_B"] = add_property_json("Saturation (Blue)", saturation_B.GetValue(requested_frame), "float", "", &saturation_B, 0.0, 4.0, false, requested_frame);

	// Return formatted string
	return root.toStyledString();
}
