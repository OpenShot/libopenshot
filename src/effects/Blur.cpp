/**
 * @file
 * @brief Source file for Blur effect class
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
std::shared_ptr<Frame> Blur::GetFrame(std::shared_ptr<Frame> frame, long int frame_number)
{
	// Get the frame's image
	std::shared_ptr<QImage> frame_image = frame->GetImage();

	// Get the current blur radius
	int horizontal_radius_value = horizontal_radius.GetValue(frame_number);
	int vertical_radius_value = vertical_radius.GetValue(frame_number);
	float sigma_value = sigma.GetValue(frame_number);
	int iteration_value = iterations.GetInt(frame_number);


	// Declare arrays for each color channel
	unsigned char *red = new unsigned char[frame_image->width() * frame_image->height()]();
	unsigned char *green = new unsigned char[frame_image->width() * frame_image->height()]();
	unsigned char *blue = new unsigned char[frame_image->width() * frame_image->height()]();
	unsigned char *alpha = new unsigned char[frame_image->width() * frame_image->height()]();
	// Create empty target RGBA arrays (for the results of our blur)
	unsigned char *blur_red = new unsigned char[frame_image->width() * frame_image->height()]();
	unsigned char *blur_green = new unsigned char[frame_image->width() * frame_image->height()]();
	unsigned char *blur_blue = new unsigned char[frame_image->width() * frame_image->height()]();
	unsigned char *blur_alpha = new unsigned char[frame_image->width() * frame_image->height()]();

	// Loop through pixels and split RGBA channels into separate arrays
	unsigned char *pixels = (unsigned char *) frame_image->bits();
	for (int pixel = 0, byte_index=0; pixel < frame_image->width() * frame_image->height(); pixel++, byte_index+=4)
	{
		// Get the RGBA values from each pixel
		unsigned char R = pixels[byte_index];
		unsigned char G = pixels[byte_index + 1];
		unsigned char B = pixels[byte_index + 2];
		unsigned char A = pixels[byte_index + 3];

		// Split channels into their own arrays
		red[pixel] = R;
		green[pixel] = G;
		blue[pixel] = B;
		alpha[pixel] = A;
	}

	// Init target RGBA arrays
	for (int i = 0; i < (frame_image->width() * frame_image->height()); i++) blur_red[i] = red[i];
	for (int i = 0; i < (frame_image->width() * frame_image->height()); i++) blur_green[i] = green[i];
	for (int i = 0; i < (frame_image->width() * frame_image->height()); i++) blur_blue[i] = blue[i];
	for (int i = 0; i < (frame_image->width() * frame_image->height()); i++) blur_alpha[i] = alpha[i];

	// Loop through each iteration
	for (int iteration = 0; iteration < iteration_value; iteration++)
	{
		// HORIZONTAL BLUR (if any)
		if (horizontal_radius_value > 0.0) {
			// Init boxes for computing blur
			int *bxs = initBoxes(sigma_value, horizontal_radius_value);

			// Apply horizontal blur to target RGBA channels
			boxBlurH(red, blur_red, frame_image->width(), frame_image->height(), horizontal_radius_value);
			boxBlurH(green, blur_green, frame_image->width(), frame_image->height(), horizontal_radius_value);
			boxBlurH(blue, blur_blue, frame_image->width(), frame_image->height(), horizontal_radius_value);
			boxBlurH(alpha, blur_alpha, frame_image->width(), frame_image->height(), horizontal_radius_value);

			// Remove boxes
			delete[] bxs;
		}

		// VERTICAL BLUR (if any)
		if (vertical_radius_value > 0.0) {
			// Init boxes for computing blur
			int *bxs = initBoxes(sigma_value, vertical_radius_value);

			// Apply vertical blur to target RGBA channels
			boxBlurT(red, blur_red, frame_image->width(), frame_image->height(), vertical_radius_value);
			boxBlurT(green, blur_green, frame_image->width(), frame_image->height(), vertical_radius_value);
			boxBlurT(blue, blur_blue, frame_image->width(), frame_image->height(), vertical_radius_value);
			boxBlurT(alpha, blur_alpha, frame_image->width(), frame_image->height(), vertical_radius_value);

			// Remove boxes
			delete[] bxs;
		}
	}

	// Copy RGBA channels back to original image
	for (int pixel = 0, byte_index=0; pixel < frame_image->width() * frame_image->height(); pixel++, byte_index+=4)
	{
		// Get the RGB values from the pixel
		unsigned char R = blur_red[pixel];
		unsigned char G = blur_green[pixel];
		unsigned char B = blur_blue[pixel];
		unsigned char A = blur_alpha[pixel];

		// Split channels into their own arrays
		pixels[byte_index] = R;
		pixels[byte_index + 1] = G;
		pixels[byte_index + 2] = B;
		pixels[byte_index + 3] = A;
	}

	// Delete channel arrays
	delete[] red;
	delete[] green;
	delete[] blue;
	delete[] alpha;
	delete[] blur_red;
	delete[] blur_green;
	delete[] blur_blue;
	delete[] blur_alpha;

	// return the modified frame
	return frame;
}

// Credit: http://blog.ivank.net/fastest-gaussian-blur.html (MIT License)
int* Blur::initBoxes(float sigma, int n)  // standard deviation, number of boxes
{
	float wIdeal = sqrt((12.0 * sigma * sigma / n) + 1.0);  // Ideal averaging filter width
	int wl = floor(wIdeal);
	if (wl % 2 == 0) wl--;
	int wu = wl + 2;

	float mIdeal = (12.0 * sigma * sigma - n * wl * wl - 4 * n * wl - 3 * n) / (-4.0 * wl - 4);
	int m = round(mIdeal);

	int *sizes = new int[n]();
	for (int i = 0; i < n; i++) sizes[i] = i < m ? wl : wu;
	return sizes;
}

// Credit: http://blog.ivank.net/fastest-gaussian-blur.html (MIT License)
void Blur::boxBlurH(unsigned char *scl, unsigned char *tcl, int w, int h, int r) {
	float iarr = 1.0 / (r + r + 1);
	for (int i = 0; i < h; i++) {
		int ti = i * w, li = ti, ri = ti + r;
		int fv = scl[ti], lv = scl[ti + w - 1], val = (r + 1) * fv;
		for (int j = 0; j < r; j++) val += scl[ti + j];
		for (int j = 0; j <= r; j++) {
			val += scl[ri++] - fv;
			tcl[ti++] = round(val * iarr);
		}
		for (int j = r + 1; j < w - r; j++) {
			val += scl[ri++] - scl[li++];
			tcl[ti++] = round(val * iarr);
		}
		for (int j = w - r; j < w; j++) {
			val += lv - scl[li++];
			tcl[ti++] = round(val * iarr);
		}
	}
}

void Blur::boxBlurT(unsigned char *scl, unsigned char *tcl, int w, int h, int r) {
	float iarr = 1.0 / (r + r + 1);
	for (int i = 0; i < w; i++) {
		int ti = i, li = ti, ri = ti + r * w;
		int fv = scl[ti], lv = scl[ti + w * (h - 1)], val = (r + 1) * fv;
		for (int j = 0; j < r; j++) val += scl[ti + j * w];
		for (int j = 0; j <= r; j++) {
			val += scl[ri] - fv;
			tcl[ti] = round(val * iarr);
			ri += w;
			ti += w;
		}
		for (int j = r + 1; j < h - r; j++) {
			val += scl[ri] - scl[li];
			tcl[ti] = round(val * iarr);
			li += w;
			ri += w;
			ti += w;
		}
		for (int j = h - r; j < h; j++) {
			val += lv - scl[li];
			tcl[ti] = round(val * iarr);
			li += w;
			ti += w;
		}
	}
}

// Generate JSON string of this object
string Blur::Json() {

	// Return formatted string
	return JsonValue().toStyledString();
}

// Generate Json::JsonValue for this object
Json::Value Blur::JsonValue() {

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
void Blur::SetJson(string value) throw(InvalidJSON) {

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
void Blur::SetJsonValue(Json::Value root) {

	// Set parent data
	EffectBase::SetJsonValue(root);

	// Set data from Json (if key is found)
	if (!root["horizontal_radius"].isNull())
		horizontal_radius.SetJsonValue(root["horizontal_radius"]);
	else if (!root["vertical_radius"].isNull())
		vertical_radius.SetJsonValue(root["vertical_radius"]);
	else if (!root["sigma"].isNull())
		sigma.SetJsonValue(root["sigma"]);
	else if (!root["iterations"].isNull())
		iterations.SetJsonValue(root["iterations"]);
}

// Get all properties for a specific frame
string Blur::PropertiesJSON(long int requested_frame) {

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

