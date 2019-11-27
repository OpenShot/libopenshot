/**
 * @file
 * @brief Source file for Pixelate effect class
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

#include "../../include/effects/Pixelate.h"

using namespace openshot;

/// Blank constructor, useful when using Json to load the effect properties
Pixelate::Pixelate() : pixelization(0.7), left(0.0), top(0.0), right(0.0), bottom(0.0) {
	// Init effect properties
	init_effect_details();
}

// Default constructor
Pixelate::Pixelate(Keyframe pixelization, Keyframe left, Keyframe top, Keyframe right, Keyframe bottom) :
	pixelization(pixelization), left(left), top(top), right(right), bottom(bottom)
{
	// Init effect properties
	init_effect_details();
}

// Init effect settings
void Pixelate::init_effect_details()
{
	/// Initialize the values of the EffectInfo struct.
	InitEffectInfo();

	/// Set the effect info
	info.class_name = "Pixelate";
	info.name = "Pixelate";
	info.description = "Pixelate (increase or decrease) the number of visible pixels.";
	info.has_audio = false;
	info.has_video = true;
}

// This method is required for all derived classes of EffectBase, and returns a
// modified openshot::Frame object
std::shared_ptr<Frame> Pixelate::GetFrame(std::shared_ptr<Frame> frame, int64_t frame_number)
{
	// Get the frame's image
	std::shared_ptr<QImage> frame_image = frame->GetImage();

	// Get current keyframe values
	double pixelization_value = 1.0 - std::min(fabs(pixelization.GetValue(frame_number)), 1.0);
	double left_value = left.GetValue(frame_number);
	double top_value = top.GetValue(frame_number);
	double right_value = right.GetValue(frame_number);
	double bottom_value = bottom.GetValue(frame_number);

	if (pixelization_value > 0.0) {
		// Resize frame image smaller (based on pixelization value)
		std::shared_ptr<QImage> smaller_frame_image = std::shared_ptr<QImage>(new QImage(frame_image->scaledToWidth(std::max(frame_image->width() * pixelization_value, 2.0), Qt::SmoothTransformation)));

		// Resize image back to original size (with no smoothing to create pixelated image)
		std::shared_ptr<QImage> pixelated_image = std::shared_ptr<QImage>(new QImage(smaller_frame_image->scaledToWidth(frame_image->width(), Qt::FastTransformation).convertToFormat(QImage::Format_RGBA8888)));

		// Get pixel array pointer
		unsigned char *pixels = (unsigned char *) frame_image->bits();
		unsigned char *pixelated_pixels = (unsigned char *) pixelated_image->bits();

		// Get pixels sizes of all margins
		int top_bar_height = top_value * frame_image->height();
		int bottom_bar_height = bottom_value * frame_image->height();
		int left_bar_width = left_value * frame_image->width();
		int right_bar_width = right_value * frame_image->width();

		// Loop through rows
		for (int row = 0; row < frame_image->height(); row++) {

			// Copy pixelated pixels into original frame image (where needed)
			if ((row >= top_bar_height) && (row <= frame_image->height() - bottom_bar_height)) {
				memcpy(&pixels[(row * frame_image->width() + left_bar_width) * 4], &pixelated_pixels[(row * frame_image->width() + left_bar_width) * 4], sizeof(char) * (frame_image->width() - left_bar_width - right_bar_width) * 4);
			}
		}

		// Cleanup temp images
		smaller_frame_image.reset();
		pixelated_image.reset();
	}

	// return the modified frame
	return frame;
}

// Generate JSON string of this object
std::string Pixelate::Json() {

	// Return formatted string
	return JsonValue().toStyledString();
}

// Generate Json::JsonValue for this object
Json::Value Pixelate::JsonValue() {

	// Create root json object
	Json::Value root = EffectBase::JsonValue(); // get parent properties
	root["type"] = info.class_name;
	root["pixelization"] = pixelization.JsonValue();
	root["left"] = left.JsonValue();
	root["top"] = top.JsonValue();
	root["right"] = right.JsonValue();
	root["bottom"] = bottom.JsonValue();

	// return JsonValue
	return root;
}

// Load JSON string into this object
void Pixelate::SetJson(std::string value) {

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
void Pixelate::SetJsonValue(Json::Value root) {

	// Set parent data
	EffectBase::SetJsonValue(root);

	// Set data from Json (if key is found)
	if (!root["pixelization"].isNull())
		pixelization.SetJsonValue(root["pixelization"]);
	if (!root["left"].isNull())
		left.SetJsonValue(root["left"]);
	if (!root["top"].isNull())
		top.SetJsonValue(root["top"]);
	if (!root["right"].isNull())
		right.SetJsonValue(root["right"]);
	if (!root["bottom"].isNull())
		bottom.SetJsonValue(root["bottom"]);
}

// Get all properties for a specific frame
std::string Pixelate::PropertiesJSON(int64_t requested_frame) {

	// Generate JSON properties list
	Json::Value root;
	root["id"] = add_property_json("ID", 0.0, "string", Id(), NULL, -1, -1, true, requested_frame);
	root["position"] = add_property_json("Position", Position(), "float", "", NULL, 0, 1000 * 60 * 30, false, requested_frame);
	root["layer"] = add_property_json("Track", Layer(), "int", "", NULL, 0, 20, false, requested_frame);
	root["start"] = add_property_json("Start", Start(), "float", "", NULL, 0, 1000 * 60 * 30, false, requested_frame);
	root["end"] = add_property_json("End", End(), "float", "", NULL, 0, 1000 * 60 * 30, false, requested_frame);
	root["duration"] = add_property_json("Duration", Duration(), "float", "", NULL, 0, 1000 * 60 * 30, true, requested_frame);

	// Keyframes
	root["pixelization"] = add_property_json("Pixelization", pixelization.GetValue(requested_frame), "float", "", &pixelization, 0.0, 0.9999, false, requested_frame);
	root["left"] = add_property_json("Left Margin", left.GetValue(requested_frame), "float", "", &left, 0.0, 1.0, false, requested_frame);
	root["top"] = add_property_json("Top Margin", top.GetValue(requested_frame), "float", "", &top, 0.0, 1.0, false, requested_frame);
	root["right"] = add_property_json("Right Margin", right.GetValue(requested_frame), "float", "", &right, 0.0, 1.0, false, requested_frame);
	root["bottom"] = add_property_json("Bottom Margin", bottom.GetValue(requested_frame), "float", "", &bottom, 0.0, 1.0, false, requested_frame);

	// Return formatted string
	return root.toStyledString();
}
