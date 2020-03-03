/**
 * @file
 * @brief Source file for Shift effect class
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

#include "../../include/effects/Shift.h"

using namespace openshot;

/// Blank constructor, useful when using Json to load the effect properties
Shift::Shift() : x(0.0), y(0.0) {
	// Init effect properties
	init_effect_details();
}

// Default constructor
Shift::Shift(Keyframe x, Keyframe y) : x(x), y(y)
{
	// Init effect properties
	init_effect_details();
}

// Init effect settings
void Shift::init_effect_details()
{
	/// Initialize the values of the EffectInfo struct.
	InitEffectInfo();

	/// Set the effect info
	info.class_name = "Shift";
	info.name = "Shift";
	info.description = "Shift the image up, down, left, and right (with infinite wrapping).";
	info.has_audio = false;
	info.has_video = true;
}

// This method is required for all derived classes of EffectBase, and returns a
// modified openshot::Frame object
std::shared_ptr<Frame> Shift::GetFrame(std::shared_ptr<Frame> frame, int64_t frame_number)
{
	// Get the frame's image
	std::shared_ptr<QImage> frame_image = frame->GetImage();
	unsigned char *pixels = (unsigned char *) frame_image->bits();

	// Get the current shift amount, and clamp to range (-1 to 1 range)
	double x_shift = x.GetValue(frame_number);
	double x_shift_limit = fmod(fabs(x_shift), 1.0);
	double y_shift = y.GetValue(frame_number);
	double y_shift_limit = fmod(fabs(y_shift), 1.0);

	// Declare temp arrays to hold pixels while we move things around
	unsigned char *temp_row = new unsigned char[frame_image->width() * 4]();

	// X-SHIFT
	// Loop through rows
	for (int row = 0; row < frame_image->height(); row++) {
		// Copy current row's pixels
		int starting_row_pixel = row * frame_image->width();
		memcpy(temp_row, &pixels[starting_row_pixel * 4], sizeof(char) * frame_image->width() * 4);

		// Replace current row with left part of the pixels
		if (x_shift > 0.0) {
			// Move left side to the right
			int relative_pixel_start = (int)round(frame_image->width() * x_shift_limit);
			memcpy(&pixels[(starting_row_pixel + relative_pixel_start) * 4], &temp_row[0], sizeof(char) * (frame_image->width() - relative_pixel_start) * 4);

			// Move right side to the left
			memcpy(&pixels[starting_row_pixel * 4], &temp_row[(frame_image->width() - relative_pixel_start) * 4], sizeof(char) * relative_pixel_start * 4);
		} else if (x_shift < 0.0) {
			// Move right side to the left
			int relative_pixel_start = (int)round(frame_image->width() * x_shift_limit);
			memcpy(&pixels[starting_row_pixel * 4], &temp_row[relative_pixel_start * 4], sizeof(char) * (frame_image->width() - relative_pixel_start) * 4);

			// Move left side to the right
			memcpy(&pixels[(starting_row_pixel + (frame_image->width() - relative_pixel_start)) * 4], &temp_row[0], sizeof(char) * relative_pixel_start * 4);
		}
	}

	// Make temp copy of pixels for Y-SHIFT
	unsigned char *temp_image = new unsigned char[frame_image->width() * frame_image->height() * 4]();
	memcpy(temp_image, pixels, sizeof(char) * frame_image->width() * frame_image->height() * 4);

	// Y-SHIFT
	// Replace current row with left part of the pixels
	if (y_shift > 0.0) {
		// Move top side to bottom
		int relative_pixel_start = frame_image->width() * (int)round(frame_image->height() * y_shift_limit);
		memcpy(&pixels[relative_pixel_start * 4], temp_image, sizeof(char) * ((frame_image->width() * frame_image->height()) - relative_pixel_start) * 4);

		// Move bottom side to top
		memcpy(pixels, &temp_image[((frame_image->width() * frame_image->height()) - relative_pixel_start) * 4], sizeof(char) * relative_pixel_start * 4);

	} else if (y_shift < 0.0) {
		// Move bottom side to top
		int relative_pixel_start = frame_image->width() * (int)round(frame_image->height() * y_shift_limit);
		memcpy(pixels, &temp_image[relative_pixel_start * 4], sizeof(char) * ((frame_image->width() * frame_image->height()) - relative_pixel_start) * 4);

		// Move left side to the right
		memcpy(&pixels[((frame_image->width() * frame_image->height()) - relative_pixel_start) * 4], temp_image, sizeof(char) * relative_pixel_start * 4);
	}

	// Delete arrays
	delete[] temp_row;
	delete[] temp_image;

	// return the modified frame
	return frame;
}

// Generate JSON string of this object
std::string Shift::Json() const {

	// Return formatted string
	return JsonValue().toStyledString();
}

// Generate Json::Value for this object
Json::Value Shift::JsonValue() const {

	// Create root json object
	Json::Value root = EffectBase::JsonValue(); // get parent properties
	root["type"] = info.class_name;
	root["x"] = x.JsonValue();
	root["y"] = y.JsonValue();

	// return JsonValue
	return root;
}

// Load JSON string into this object
void Shift::SetJson(const std::string value) {

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
void Shift::SetJsonValue(const Json::Value root) {

	// Set parent data
	EffectBase::SetJsonValue(root);

	// Set data from Json (if key is found)
	if (!root["x"].isNull())
		x.SetJsonValue(root["x"]);
	if (!root["y"].isNull())
		y.SetJsonValue(root["y"]);
}

// Get all properties for a specific frame
std::string Shift::PropertiesJSON(int64_t requested_frame) const {

	// Generate JSON properties list
	Json::Value root;
	root["id"] = add_property_json("ID", 0.0, "string", Id(), NULL, -1, -1, true, requested_frame);
	root["position"] = add_property_json("Position", Position(), "float", "", NULL, 0, 1000 * 60 * 30, false, requested_frame);
	root["layer"] = add_property_json("Track", Layer(), "int", "", NULL, 0, 20, false, requested_frame);
	root["start"] = add_property_json("Start", Start(), "float", "", NULL, 0, 1000 * 60 * 30, false, requested_frame);
	root["end"] = add_property_json("End", End(), "float", "", NULL, 0, 1000 * 60 * 30, false, requested_frame);
	root["duration"] = add_property_json("Duration", Duration(), "float", "", NULL, 0, 1000 * 60 * 30, true, requested_frame);

	// Keyframes
	root["x"] = add_property_json("X Shift", x.GetValue(requested_frame), "float", "", &x, -1, 1, false, requested_frame);
	root["y"] = add_property_json("Y Shift", y.GetValue(requested_frame), "float", "", &y, -1, 1, false, requested_frame);

	// Return formatted string
	return root.toStyledString();
}
