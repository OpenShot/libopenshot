/**
 * @file
 * @brief Source file for Outline effect class
 * @author Jonathan Thomas <jonathan@openshot.org>
 * @author HaiVQ <me@haivq.com>
 *
 * @ref License
 */

// Copyright (c) 2008-2025 OpenShot Studios, LLC
//
// SPDX-License-Identifier: LGPL-3.0-or-later

#include "Shadow.h"
#include "Exceptions.h"

using namespace openshot;

/// Blank constructor, useful when using Json to load the effect properties
Shadow::Shadow() : x_offset(10), y_offset(10), blur_radius(10.0) {
	// Init effect properties
	color = Color("#000000");
	color.alpha = 128; // alpha = 0.5
	init_effect_details();
}

// Default constructor
Shadow::Shadow(Keyframe x_offset, Keyframe y_offset, Keyframe blur_radius, Color color) :
	x_offset(x_offset), y_offset(y_offset), blur_radius(blur_radius), color(color)
{
	// Init effect properties
	init_effect_details();
}

// Init effect settings
void Shadow::init_effect_details()
{
	/// Initialize the values of the EffectInfo struct.
	InitEffectInfo();

	/// Set the effect info
	info.class_name = "Shadow";
	info.name = "Shadow";
	info.description = "Drop shadow under any image or text.";
	info.has_audio = false;
	info.has_video = true;
}

// This method is required for all derived classes of EffectBase, and returns a
// modified openshot::Frame object
std::shared_ptr<openshot::Frame> Shadow::GetFrame(std::shared_ptr<openshot::Frame> frame, int64_t frame_number)
{
	int x_offsetValue = x_offset.GetValue(frame_number);
	int y_offsetValue = y_offset.GetValue(frame_number);
	int blur_radiusValue = blur_radius.GetValue(frame_number);

	int blueValue = color.blue.GetValue(frame_number);
	int greenValue = color.green.GetValue(frame_number);
	int redValue = color.red.GetValue(frame_number);
	int alphaValue = color.alpha.GetValue(frame_number);
	
	// The shadow drop directly under the image or completely transparent.
	// No need to do anything here, return the original frame
	if (
		((x_offsetValue == 0.0) && (y_offsetValue == 0) && (blur_radiusValue == 0.0)) // shadow is directly under the image
		||
		(alphaValue <= 0) // shadow is completely transparent
	) {
		return frame;
	}

	// Get the frame's image
	cv::Mat cv_image = frame->GetBGRACvMat();
	
	int abs_x_offset = abs(x_offsetValue);
	int abs_y_offset = abs(y_offsetValue);
	int paddedWidth = cv_image.cols + 2 * abs_x_offset;
	int paddedHeight = cv_image.rows + 2 * abs_y_offset;

	// The shadow is completely out of the frame
	if ((abs_x_offset + blur_radiusValue > cv_image.cols) || (abs_y_offset + blur_radiusValue > cv_image.rows)) {
		return frame;
	}

	std::vector<cv::Mat> channels(4);
	cv::split(cv_image, channels);

	// Prepare shadow mask
	cv::Mat shadow_mask = channels[3].clone();

	cv::Mat final_image;

	// Padding the shadow color matrix and shadow mask to use ROI later
	cv::Mat shadow_color_mat(cv::Size(paddedWidth, paddedHeight), CV_8UC4, cv::Scalar(redValue, greenValue, blueValue, alphaValue));

	// only shifting image if shadow is not directly under the image
	if (abs_x_offset && abs_y_offset) {
		cv::copyMakeBorder(shadow_mask, shadow_mask, abs_y_offset, abs_y_offset, abs_x_offset, abs_x_offset, cv::BorderTypes::BORDER_REFLECT);
		// Create ROI to crop from the shadow color matrix and shadow mask above,
		// shift the shadow color and shadow mask
		cv::Rect roi(abs_x_offset - x_offsetValue, abs_y_offset - y_offsetValue, cv_image.cols, cv_image.rows);

		// Draw cropped (by ROI) shadow color mat into final image
		shadow_color_mat(roi).copyTo(final_image, shadow_mask(roi));
	} else {
		// Draw shadow color mat into final image
		shadow_color_mat.copyTo(final_image, shadow_mask);
	}

	// Blur the final image to simulate shadow blur. Ignore if blur_radius is 0
	// FIXME: Not physically correct
	if (blur_radiusValue > 0.0) {
		cv::GaussianBlur(final_image, final_image, cv::Size(0, 0), blur_radiusValue, blur_radiusValue, cv::BorderTypes::BORDER_DEFAULT);
	}

	// Draw the original image on top of the shadow
	cv_image.copyTo(final_image, channels[3]);

	frame->SetBGRACvMat(final_image);

	return frame;
}

// Generate JSON string of this object
std::string Shadow::Json() const {

	// Return formatted string
	return JsonValue().toStyledString();
}

// Generate Json::Value for this object
Json::Value Shadow::JsonValue() const {

	// Create root json object
	Json::Value root = EffectBase::JsonValue(); // get parent properties
	root["type"] = info.class_name;
	root["x_offset"] = x_offset.JsonValue();
	root["y_offset"] = y_offset.JsonValue();
	root["blur_radius"] = blur_radius.JsonValue();
	root["color"] = color.JsonValue();

	// return JsonValue
	return root;
}

// Load JSON string into this object
void Shadow::SetJson(const std::string value) {

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
void Shadow::SetJsonValue(const Json::Value root) {

	// Set parent data
	EffectBase::SetJsonValue(root);

	// Set data from Json (if key is found)
	if (!root["x_offset"].isNull())
		x_offset.SetJsonValue(root["x_offset"]);
	if (!root["y_offset"].isNull())
		y_offset.SetJsonValue(root["y_offset"]);
	if (!root["blur_radius"].isNull())
		blur_radius.SetJsonValue(root["blur_radius"]);
	if (!root["color"].isNull())
		color.SetJsonValue(root["color"]);
}

// Get all properties for a specific frame
std::string Shadow::PropertiesJSON(int64_t requested_frame) const {

	// Generate JSON properties list
	Json::Value root = BasePropertiesJSON(requested_frame);

	// Keyframes
	root["x_offset"] = add_property_json("X Offset", x_offset.GetValue(requested_frame), "int", "", &x_offset, -1000, 1000, false, requested_frame);
	root["y_offset"] = add_property_json("Y Offset", y_offset.GetValue(requested_frame), "int", "", &y_offset, -1000, 1000, false, requested_frame);
	root["blur_radius"] = add_property_json("Blur Radius", blur_radius.GetValue(requested_frame), "int", "", &blur_radius, 0, 1000, false, requested_frame);
	root["color"] = add_property_json("Key Color", 0.0, "color", "", &color.red, 0, 255, false, requested_frame);
	root["color"]["red"] = add_property_json("Red", color.red.GetValue(requested_frame), "float", "", &color.red, 0, 255, false, requested_frame);
	root["color"]["green"] = add_property_json("Green", color.green.GetValue(requested_frame), "float", "", &color.green, 0, 255, false, requested_frame);
	root["color"]["blue"] = add_property_json("Blue", color.blue.GetValue(requested_frame), "float", "", &color.blue, 0, 255, false, requested_frame);
	root["color"]["alpha"] = add_property_json("Alpha", color.alpha.GetValue(requested_frame), "float", "", &color.alpha, 0, 255, false, requested_frame);

	// Return formatted string
	return root.toStyledString();
}
