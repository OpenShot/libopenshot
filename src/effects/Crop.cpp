/**
 * @file
 * @brief Source file for Crop effect class (cropping any side, with x/y offsets)
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

#include "Crop.h"
#include "Exceptions.h"

using namespace openshot;

/// Blank constructor, useful when using Json to load the effect properties
Crop::Crop() : left(0.0), top(0.0), right(0.0), bottom(0.0), x(0.0), y(0.0) {
	// Init effect properties
	init_effect_details();
}

// Default constructor
Crop::Crop(Keyframe left, Keyframe top, Keyframe right, Keyframe bottom) :
		left(left), top(top), right(right), bottom(bottom), x(0.0), y(0.0)
{
	// Init effect properties
	init_effect_details();
}

// Init effect settings
void Crop::init_effect_details()
{
	/// Initialize the values of the EffectInfo struct.
	InitEffectInfo();

	/// Set the effect info
	info.class_name = "Crop";
	info.name = "Crop";
	info.description = "Crop out any part of your video.";
	info.has_audio = false;
	info.has_video = true;
}

// This method is required for all derived classes of EffectBase, and returns a
// modified openshot::Frame object
std::shared_ptr<openshot::Frame> Crop::GetFrame(std::shared_ptr<openshot::Frame> frame, int64_t frame_number)
{
	// Get the frame's image
	std::shared_ptr<QImage> frame_image = frame->GetImage();

    // Get transparent color target image (which will become the cropped image)
    auto cropped_image = std::make_shared<QImage>(
            frame_image->width(), frame_image->height(), QImage::Format_RGBA8888_Premultiplied);
    cropped_image->fill(QColor(QString::fromStdString("transparent")));

	// Get current keyframe values
	double left_value = left.GetValue(frame_number);
	double top_value = top.GetValue(frame_number);
	double right_value = right.GetValue(frame_number);
	double bottom_value = bottom.GetValue(frame_number);

    // Get the current shift amount (if any... to slide the image around in the cropped area)
    double x_shift = x.GetValue(frame_number);
    double y_shift = y.GetValue(frame_number);

	// Get pixel array pointers
	unsigned char *pixels = (unsigned char *) frame_image->bits();
    unsigned char *cropped_pixels = (unsigned char *) cropped_image->bits();

	// Get pixels sizes of all crop sides
	int top_bar_height = top_value * frame_image->height();
	int bottom_bar_height = bottom_value * frame_image->height();
	int left_bar_width = left_value * frame_image->width();
	int right_bar_width = right_value * frame_image->width();
	int column_offset = x_shift * frame_image->width();
	int row_offset = y_shift * frame_image->height();

	// Image copy variables
	int image_width = frame_image->width();
    int src_start = left_bar_width;
    int dst_start = left_bar_width;
    int copy_length = frame_image->width() - right_bar_width - left_bar_width;

    // Adjust for x offset
    int copy_offset = 0;

    if (column_offset < 0) {
        // dest to the right
        src_start += column_offset;
        if (src_start < 0) {
            int diff = 0 - src_start; // how far under 0 are we?
            src_start = 0;
            dst_start += diff;
            copy_offset = -diff;
        } else {
            copy_offset = 0;
        }

    } else {
        // dest to the left
        src_start += column_offset;
        if (image_width - src_start >= copy_length) {
            // We have plenty pixels, use original copy-length
            copy_offset = 0;
        } else {
            // We don't have enough pixels, shorten copy-length
            copy_offset = (image_width - src_start) - copy_length;
        }
    }

	// Loop through rows of pixels
	for (int row = 0; row < frame_image->height(); row++) {
        int adjusted_row = row - row_offset;
	    // Is this row visible?
        if (adjusted_row >= top_bar_height && adjusted_row < (frame_image->height() - bottom_bar_height) && (copy_length + copy_offset > 0)) {
            // Copy image (row by row, with offsets for x and y offset, and src/dst starting points for column filtering)
            memcpy(&cropped_pixels[((adjusted_row * frame_image->width()) + dst_start) * 4],
                   &pixels[((row * frame_image->width()) + src_start) * 4],
                   sizeof(char) * (copy_length + copy_offset) * 4);
		}
	}

	// Set frame image
	frame->AddImage(cropped_image);

	// return the modified frame
	return frame;
}

// Generate JSON string of this object
std::string Crop::Json() const {

	// Return formatted string
	return JsonValue().toStyledString();
}

// Generate Json::Value for this object
Json::Value Crop::JsonValue() const {

	// Create root json object
	Json::Value root = EffectBase::JsonValue(); // get parent properties
	root["type"] = info.class_name;
	root["left"] = left.JsonValue();
	root["top"] = top.JsonValue();
	root["right"] = right.JsonValue();
	root["bottom"] = bottom.JsonValue();
    root["x"] = x.JsonValue();
    root["y"] = y.JsonValue();

	// return JsonValue
	return root;
}

// Load JSON string into this object
void Crop::SetJson(const std::string value) {

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
void Crop::SetJsonValue(const Json::Value root) {

	// Set parent data
	EffectBase::SetJsonValue(root);

	// Set data from Json (if key is found)
	if (!root["left"].isNull())
		left.SetJsonValue(root["left"]);
	if (!root["top"].isNull())
		top.SetJsonValue(root["top"]);
	if (!root["right"].isNull())
		right.SetJsonValue(root["right"]);
	if (!root["bottom"].isNull())
		bottom.SetJsonValue(root["bottom"]);
    if (!root["x"].isNull())
        x.SetJsonValue(root["x"]);
    if (!root["y"].isNull())
        y.SetJsonValue(root["y"]);
}

// Get all properties for a specific frame
std::string Crop::PropertiesJSON(int64_t requested_frame) const {

	// Generate JSON properties list
	Json::Value root;
	root["id"] = add_property_json("ID", 0.0, "string", Id(), NULL, -1, -1, true, requested_frame);
	root["position"] = add_property_json("Position", Position(), "float", "", NULL, 0, 1000 * 60 * 30, false, requested_frame);
	root["layer"] = add_property_json("Track", Layer(), "int", "", NULL, 0, 20, false, requested_frame);
	root["start"] = add_property_json("Start", Start(), "float", "", NULL, 0, 1000 * 60 * 30, false, requested_frame);
	root["end"] = add_property_json("End", End(), "float", "", NULL, 0, 1000 * 60 * 30, false, requested_frame);
	root["duration"] = add_property_json("Duration", Duration(), "float", "", NULL, 0, 1000 * 60 * 30, true, requested_frame);

	// Keyframes
	root["left"] = add_property_json("Left Size", left.GetValue(requested_frame), "float", "", &left, 0.0, 1.0, false, requested_frame);
	root["top"] = add_property_json("Top Size", top.GetValue(requested_frame), "float", "", &top, 0.0, 1.0, false, requested_frame);
	root["right"] = add_property_json("Right Size", right.GetValue(requested_frame), "float", "", &right, 0.0, 1.0, false, requested_frame);
	root["bottom"] = add_property_json("Bottom Size", bottom.GetValue(requested_frame), "float", "", &bottom, 0.0, 1.0, false, requested_frame);
    root["x"] = add_property_json("X Offset", x.GetValue(requested_frame), "float", "", &x, -1.0, 1.0, false, requested_frame);
    root["y"] = add_property_json("Y Offset", y.GetValue(requested_frame), "float", "", &y, -1.0, 1.0, false, requested_frame);

	// Set the parent effect which properties this effect will inherit
	root["parent_effect_id"] = add_property_json("Parent", 0.0, "string", info.parent_effect_id, NULL, -1, -1, false, requested_frame);

	// Return formatted string
	return root.toStyledString();
}
