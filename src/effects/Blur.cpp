/**
 * @file
 * @brief Source file for Blur effect class
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

#include "../../include/effects/Blur.h"

using namespace openshot;

/// Blank constructor, useful when using Json to load the effect properties
Blur::Blur() : horizontal_radius(6.0), vertical_radius(6.0), sigma(3.0), iterations(3.0) {
	// Init effect properties
	init_effect_details();
}

// Default constructor
Blur::Blur(Keyframe new_horizontal_radius, Keyframe new_vertical_radius, Keyframe new_sigma, Keyframe new_iterations) :
		horizontal_radius(new_horizontal_radius), vertical_radius(new_vertical_radius),
		sigma(new_sigma), iterations(new_iterations)
{
	// Init effect properties
	init_effect_details();
}

// Init effect settings
void Blur::init_effect_details()
{
	/// Initialize the values of the EffectInfo struct.
	InitEffectInfo();

	/// Set the effect info
	info.class_name = "Blur";
	info.name = "Blur";
	info.description = "Adjust the blur of the frame's image.";
	info.has_audio = false;
	info.has_video = true;
}

// This method is required for all derived classes of EffectBase, and returns a
// modified openshot::Frame object
std::shared_ptr<Frame> Blur::GetFrame(std::shared_ptr<Frame> frame, int64_t frame_number)
{
	// Get the frame's image
	std::shared_ptr<QImage> frame_image = frame->GetImage();

	// Get the current blur radius
	int horizontal_radius_value = horizontal_radius.GetValue(frame_number);
	int vertical_radius_value = vertical_radius.GetValue(frame_number);
	float sigma_value = sigma.GetValue(frame_number);
	int iteration_value = iterations.GetInt(frame_number);

	int w = frame_image->width();
	int h = frame_image->height();

	// Grab two copies of the image pixel data
	QImage image_copy = frame_image->copy();
	std::shared_ptr<QImage> frame_image_2 = std::make_shared<QImage>(image_copy);

	// Loop through each iteration
	for (int iteration = 0; iteration < iteration_value; ++iteration)
	{
		// HORIZONTAL BLUR (if any)
		if (horizontal_radius_value > 0.0) {
			// Apply horizontal blur to target RGBA channels
			boxBlurH(frame_image->bits(), frame_image_2->bits(), w, h, horizontal_radius_value);

			// Swap output image back to input
			frame_image.swap(frame_image_2);
		}

		// VERTICAL BLUR (if any)
		if (vertical_radius_value > 0.0) {
			// Apply vertical blur to target RGBA channels
			boxBlurT(frame_image->bits(), frame_image_2->bits(), w, h, vertical_radius_value);

			// Swap output image back to input
			frame_image.swap(frame_image_2);
		}
	}

	// return the modified frame
	return frame;
}

// Credit: http://blog.ivank.net/fastest-gaussian-blur.html (MIT License)
// Modified to process all four channels in a pixel array
void Blur::boxBlurH(unsigned char *scl, unsigned char *tcl, int w, int h, int r) {
	float iarr = 1.0 / (r + r + 1);

	#pragma omp parallel for shared (scl, tcl)
	for (int i = 0; i < h; ++i) {
		for (int ch = 0; ch < 4; ++ch) {
			int ti = i * w, li = ti, ri = ti + r;
			int fv = scl[ti * 4 + ch], lv = scl[(ti + w - 1) * 4 + ch], val = (r + 1) * fv;
			for (int j = 0; j < r; ++j) {
				val += scl[(ti + j) * 4 + ch];
			}
			for (int j = 0; j <= r; ++j) {
				val += scl[ri++ * 4 + ch] - fv;
				tcl[ti++ * 4 + ch] = round(val * iarr);
			}
			for (int j = r + 1; j < w - r; ++j) {
				val += scl[ri++ * 4 + ch] - scl[li++ * 4 + ch];
				tcl[ti++ * 4 + ch] = round(val * iarr);
			}
			for (int j = w - r; j < w; ++j) {
				val += lv - scl[li++ * 4 + ch];
				tcl[ti++ * 4 + ch] = round(val * iarr);
			}
		}
	}
}

void Blur::boxBlurT(unsigned char *scl, unsigned char *tcl, int w, int h, int r) {
	float iarr = 1.0 / (r + r + 1);

	#pragma omp parallel for shared (scl, tcl)
	for (int i = 0; i < w; i++) {
		for (int ch = 0; ch < 4; ++ch) {
			int ti = i, li = ti, ri = ti + r * w;
			int fv = scl[ti * 4 + ch], lv = scl[(ti + w * (h - 1)) * 4 + ch], val = (r + 1) * fv;
			for (int j = 0; j < r; j++) val += scl[(ti + j * w) * 4 + ch];
			for (int j = 0; j <= r; j++) {
				val += scl[ri * 4 + ch] - fv;
				tcl[ti * 4 + ch] = round(val * iarr);
				ri += w;
				ti += w;
			}
			for (int j = r + 1; j < h - r; j++) {
				val += scl[ri * 4 + ch] - scl[li * 4 + ch];
				tcl[ti * 4 + ch] = round(val * iarr);
				li += w;
				ri += w;
				ti += w;
			}
			for (int j = h - r; j < h; j++) {
				val += lv - scl[li * 4 + ch];
				tcl[ti * 4 + ch] = round(val * iarr);
				li += w;
				ti += w;
			}
		}
	}
}

// Generate JSON string of this object
std::string Blur::Json() const {

	// Return formatted string
	return JsonValue().toStyledString();
}

// Generate Json::Value for this object
Json::Value Blur::JsonValue() const {

	// Create root json object
	Json::Value root = EffectBase::JsonValue(); // get parent properties
	root["type"] = info.class_name;
	root["horizontal_radius"] = horizontal_radius.JsonValue();
	root["vertical_radius"] = vertical_radius.JsonValue();
	root["sigma"] = sigma.JsonValue();
	root["iterations"] = iterations.JsonValue();

	// return JsonValue
	return root;
}

// Load JSON string into this object
void Blur::SetJson(const std::string value) {

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
void Blur::SetJsonValue(const Json::Value root) {

	// Set parent data
	EffectBase::SetJsonValue(root);

	// Set data from Json (if key is found)
	if (!root["horizontal_radius"].isNull())
		horizontal_radius.SetJsonValue(root["horizontal_radius"]);
	if (!root["vertical_radius"].isNull())
		vertical_radius.SetJsonValue(root["vertical_radius"]);
	if (!root["sigma"].isNull())
		sigma.SetJsonValue(root["sigma"]);
	if (!root["iterations"].isNull())
		iterations.SetJsonValue(root["iterations"]);
}

// Get all properties for a specific frame
std::string Blur::PropertiesJSON(int64_t requested_frame) const {

	// Generate JSON properties list
	Json::Value root;
	root["id"] = add_property_json("ID", 0.0, "string", Id(), NULL, -1, -1, true, requested_frame);
	root["position"] = add_property_json("Position", Position(), "float", "", NULL, 0, 1000 * 60 * 30, false, requested_frame);
	root["layer"] = add_property_json("Track", Layer(), "int", "", NULL, 0, 20, false, requested_frame);
	root["start"] = add_property_json("Start", Start(), "float", "", NULL, 0, 1000 * 60 * 30, false, requested_frame);
	root["end"] = add_property_json("End", End(), "float", "", NULL, 0, 1000 * 60 * 30, false, requested_frame);
	root["duration"] = add_property_json("Duration", Duration(), "float", "", NULL, 0, 1000 * 60 * 30, true, requested_frame);

	// Keyframes
	root["horizontal_radius"] = add_property_json("Horizontal Radius", horizontal_radius.GetValue(requested_frame), "float", "", &horizontal_radius, 0, 100, false, requested_frame);
	root["vertical_radius"] = add_property_json("Vertical Radius", vertical_radius.GetValue(requested_frame), "float", "", &vertical_radius, 0, 100, false, requested_frame);
	root["sigma"] = add_property_json("Sigma", sigma.GetValue(requested_frame), "float", "", &sigma, 0, 100, false, requested_frame);
	root["iterations"] = add_property_json("Iterations", iterations.GetValue(requested_frame), "float", "", &iterations, 0, 100, false, requested_frame);

	// Return formatted string
	return root.toStyledString();
}
