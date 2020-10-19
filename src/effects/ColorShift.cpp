/**
 * @file
 * @brief Source file for Color Shift effect class
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

#include "ColorShift.h"

using namespace openshot;

/// Blank constructor, useful when using Json to load the effect properties
ColorShift::ColorShift() : red_x(0.0), red_y(0.0), green_x(0.0), green_y(0.0), blue_x(0.0), blue_y(0.0), alpha_x(0.0), alpha_y(0.0) {
	// Init effect properties
	init_effect_details();
}

// Default constructor
ColorShift::ColorShift(Keyframe red_x, Keyframe red_y, Keyframe green_x, Keyframe green_y, Keyframe blue_x, Keyframe blue_y, Keyframe alpha_x, Keyframe alpha_y) :
		red_x(red_x), red_y(red_y), green_x(green_x), green_y(green_y), blue_x(blue_x), blue_y(blue_y), alpha_x(alpha_x), alpha_y(alpha_y)
{
	// Init effect properties
	init_effect_details();
}

// Init effect settings
void ColorShift::init_effect_details()
{
	/// Initialize the values of the EffectInfo struct.
	InitEffectInfo();

	/// Set the effect info
	info.class_name = "ColorShift";
	info.name = "Color Shift";
	info.description = "Shift the colors of an image up, down, left, and right (with infinite wrapping).";
	info.has_audio = false;
	info.has_video = true;
}

// This method is required for all derived classes of EffectBase, and returns a
// modified openshot::Frame object
std::shared_ptr<Frame> ColorShift::GetFrame(std::shared_ptr<Frame> frame, int64_t frame_number)
{
	// Get the frame's image
	std::shared_ptr<QImage> frame_image = frame->GetImage();
	unsigned char *pixels = (unsigned char *) frame_image->bits();

	// Get image size
	int frame_image_width = frame_image->width();
	int frame_image_height = frame_image->height();

	// Get the current shift amount, and clamp to range (-1 to 1 range)
	// Red Keyframes
	float red_x_shift = red_x.GetValue(frame_number);
	int red_x_shift_limit = round(frame_image_width * fmod(fabs(red_x_shift), 1.0));
	float red_y_shift = red_y.GetValue(frame_number);
	int red_y_shift_limit = round(frame_image_height * fmod(fabs(red_y_shift), 1.0));
	// Green Keyframes
	float green_x_shift = green_x.GetValue(frame_number);
	int green_x_shift_limit = round(frame_image_width * fmod(fabs(green_x_shift), 1.0));
	float green_y_shift = green_y.GetValue(frame_number);
	int green_y_shift_limit = round(frame_image_height * fmod(fabs(green_y_shift), 1.0));
	// Blue Keyframes
	float blue_x_shift = blue_x.GetValue(frame_number);
	int blue_x_shift_limit = round(frame_image_width * fmod(fabs(blue_x_shift), 1.0));
	float blue_y_shift = blue_y.GetValue(frame_number);
	int blue_y_shift_limit = round(frame_image_height * fmod(fabs(blue_y_shift), 1.0));
	// Alpha Keyframes
	float alpha_x_shift = alpha_x.GetValue(frame_number);
	int alpha_x_shift_limit = round(frame_image_width * fmod(fabs(alpha_x_shift), 1.0));
	float alpha_y_shift = alpha_y.GetValue(frame_number);
	int alpha_y_shift_limit = round(frame_image_height * fmod(fabs(alpha_y_shift), 1.0));

	// Make temp copy of pixels
	unsigned char *temp_image = new unsigned char[frame_image_width * frame_image_height * 4]();
	memcpy(temp_image, pixels, sizeof(char) * frame_image_width * frame_image_height * 4);

	// Init position of current row and pixel
	int starting_row_index = 0;
	int byte_index = 0;

	// Init RGBA values
	unsigned char R = 0;
	unsigned char G = 0;
	unsigned char B = 0;
	unsigned char A = 0;

	int red_starting_row_index = 0;
	int green_starting_row_index = 0;
	int blue_starting_row_index = 0;
	int alpha_starting_row_index = 0;

	int red_pixel_offset = 0;
	int green_pixel_offset = 0;
	int blue_pixel_offset = 0;
	int alpha_pixel_offset = 0;

	// Loop through rows of pixels
	for (int row = 0; row < frame_image_height; row++) {
		for (int col = 0; col < frame_image_width; col++) {
			// Get position of current row and pixel
			starting_row_index = row * frame_image_width * 4;
			byte_index = starting_row_index + (col * 4);
			red_starting_row_index = starting_row_index;
			green_starting_row_index = starting_row_index;
			blue_starting_row_index = starting_row_index;
			alpha_starting_row_index = starting_row_index;

			red_pixel_offset = 0;
			green_pixel_offset = 0;
			blue_pixel_offset = 0;
			alpha_pixel_offset = 0;

			// Get the RGBA value from each pixel (depending on offset)
			R = temp_image[byte_index];
			G = temp_image[byte_index + 1];
			B = temp_image[byte_index + 2];
			A = temp_image[byte_index + 3];

			// Shift X
			if (red_x_shift > 0.0)
				red_pixel_offset = (col + red_x_shift_limit) % frame_image_width;
			if (red_x_shift < 0.0)
				red_pixel_offset = (frame_image_width + col - red_x_shift_limit) % frame_image_width;
			if (green_x_shift > 0.0)
				green_pixel_offset = (col + green_x_shift_limit) % frame_image_width;
			if (green_x_shift < 0.0)
				green_pixel_offset = (frame_image_width + col - green_x_shift_limit) % frame_image_width;
			if (blue_x_shift > 0.0)
				blue_pixel_offset = (col + blue_x_shift_limit) % frame_image_width;
			if (blue_x_shift < 0.0)
				blue_pixel_offset = (frame_image_width + col - blue_x_shift_limit) % frame_image_width;
			if (alpha_x_shift > 0.0)
				alpha_pixel_offset = (col + alpha_x_shift_limit) % frame_image_width;
			if (alpha_x_shift < 0.0)
				alpha_pixel_offset = (frame_image_width + col - alpha_x_shift_limit) % frame_image_width;

			// Shift Y
			if (red_y_shift > 0.0)
				red_starting_row_index = ((row + red_y_shift_limit) % frame_image_height) * frame_image_width * 4;
			if (red_y_shift < 0.0)
				red_starting_row_index = ((frame_image_height + row - red_y_shift_limit) % frame_image_height) * frame_image_width * 4;
			if (green_y_shift > 0.0)
				green_starting_row_index = ((row + green_y_shift_limit) % frame_image_height) * frame_image_width * 4;
			if (green_y_shift < 0.0)
				green_starting_row_index = ((frame_image_height + row - green_y_shift_limit) % frame_image_height) * frame_image_width * 4;
			if (blue_y_shift > 0.0)
				blue_starting_row_index = ((row + blue_y_shift_limit) % frame_image_height) * frame_image_width * 4;
			if (blue_y_shift < 0.0)
				blue_starting_row_index = ((frame_image_height + row - blue_y_shift_limit) % frame_image_height) * frame_image_width * 4;
			if (alpha_y_shift > 0.0)
				alpha_starting_row_index = ((row + alpha_y_shift_limit) % frame_image_height) * frame_image_width * 4;
			if (alpha_y_shift < 0.0)
				alpha_starting_row_index = ((frame_image_height + row - alpha_y_shift_limit) % frame_image_height) * frame_image_width * 4;

			// Copy new values to this pixel
			pixels[red_starting_row_index + 0 + (red_pixel_offset * 4)] = R;
			pixels[green_starting_row_index + 1 + (green_pixel_offset * 4)] = G;
			pixels[blue_starting_row_index + 2 + (blue_pixel_offset * 4)] = B;
			pixels[alpha_starting_row_index + 3 + (alpha_pixel_offset * 4)] = A;
		}
	}

	// Delete arrays
	delete[] temp_image;

	// return the modified frame
	return frame;
}

// Generate JSON string of this object
std::string ColorShift::Json() const {

	// Return formatted string
	return JsonValue().toStyledString();
}

// Generate Json::Value for this object
Json::Value ColorShift::JsonValue() const {

	// Create root json object
	Json::Value root = EffectBase::JsonValue(); // get parent properties
	root["type"] = info.class_name;
	root["red_x"] = red_x.JsonValue();
	root["red_y"] = red_y.JsonValue();
	root["green_x"] = green_x.JsonValue();
	root["green_y"] = green_y.JsonValue();
	root["blue_x"] = blue_x.JsonValue();
	root["blue_y"] = blue_y.JsonValue();
	root["alpha_x"] = alpha_x.JsonValue();
	root["alpha_y"] = alpha_y.JsonValue();

	// return JsonValue
	return root;
}

// Load JSON string into this object
void ColorShift::SetJson(const std::string value) {

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
void ColorShift::SetJsonValue(const Json::Value root) {

	// Set parent data
	EffectBase::SetJsonValue(root);

	// Set data from Json (if key is found)
	if (!root["red_x"].isNull())
		red_x.SetJsonValue(root["red_x"]);
	if (!root["red_y"].isNull())
		red_y.SetJsonValue(root["red_y"]);
	if (!root["green_x"].isNull())
		green_x.SetJsonValue(root["green_x"]);
	if (!root["green_y"].isNull())
		green_y.SetJsonValue(root["green_y"]);
	if (!root["blue_x"].isNull())
		blue_x.SetJsonValue(root["blue_x"]);
	if (!root["blue_y"].isNull())
		blue_y.SetJsonValue(root["blue_y"]);
	if (!root["alpha_x"].isNull())
		alpha_x.SetJsonValue(root["alpha_x"]);
	if (!root["alpha_y"].isNull())
		alpha_y.SetJsonValue(root["alpha_y"]);
}

// Get all properties for a specific frame
std::string ColorShift::PropertiesJSON(int64_t requested_frame) const {

	// Generate JSON properties list
	Json::Value root;
	root["id"] = add_property_json("ID", 0.0, "string", Id(), NULL, -1, -1, true, requested_frame);
	root["position"] = add_property_json("Position", Position(), "float", "", NULL, 0, 1000 * 60 * 30, false, requested_frame);
	root["layer"] = add_property_json("Track", Layer(), "int", "", NULL, 0, 20, false, requested_frame);
	root["start"] = add_property_json("Start", Start(), "float", "", NULL, 0, 1000 * 60 * 30, false, requested_frame);
	root["end"] = add_property_json("End", End(), "float", "", NULL, 0, 1000 * 60 * 30, false, requested_frame);
	root["duration"] = add_property_json("Duration", Duration(), "float", "", NULL, 0, 1000 * 60 * 30, true, requested_frame);

	// Keyframes
	root["red_x"] = add_property_json("Red X Shift", red_x.GetValue(requested_frame), "float", "", &red_x, -1, 1, false, requested_frame);
	root["red_y"] = add_property_json("Red Y Shift", red_y.GetValue(requested_frame), "float", "", &red_y, -1, 1, false, requested_frame);
	root["green_x"] = add_property_json("Green X Shift", green_x.GetValue(requested_frame), "float", "", &green_x, -1, 1, false, requested_frame);
	root["green_y"] = add_property_json("Green Y Shift", green_y.GetValue(requested_frame), "float", "", &green_y, -1, 1, false, requested_frame);
	root["blue_x"] = add_property_json("Blue X Shift", blue_x.GetValue(requested_frame), "float", "", &blue_x, -1, 1, false, requested_frame);
	root["blue_y"] = add_property_json("Blue Y Shift", blue_y.GetValue(requested_frame), "float", "", &blue_y, -1, 1, false, requested_frame);
	root["alpha_x"] = add_property_json("Alpha X Shift", alpha_x.GetValue(requested_frame), "float", "", &alpha_x, -1, 1, false, requested_frame);
	root["alpha_y"] = add_property_json("Alpha Y Shift", alpha_y.GetValue(requested_frame), "float", "", &alpha_y, -1, 1, false, requested_frame);

	// Return formatted string
	return root.toStyledString();
}
