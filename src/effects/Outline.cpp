/**
 * @file
 * @brief Source file for Outline effect class
 * @author Jonathan Thomas <jonathan@openshot.org>
 * @author HaiVQ <me@haivq.com>
 * 
 * @ref License
 */

// Copyright (c) 2008-2019 OpenShot Studios, LLC
//
// SPDX-License-Identifier: LGPL-3.0-or-later

#include "Outline.h"
#include "Exceptions.h"

using namespace openshot;

/// Blank constructor, useful when using Json to load the effect properties
Outline::Outline() : width(0.0) {
	// Init effect properties
	color = Color("#FFFFFF");
	init_effect_details();
}

// Default constructor
Outline::Outline(Keyframe width, Color color) :
	width(width), color(color)
{
	// Init effect properties
	init_effect_details();
}

// Init effect settings
void Outline::init_effect_details()
{
	/// Initialize the values of the EffectInfo struct.
	InitEffectInfo();

	/// Set the effect info
	info.class_name = "Outline";
	info.name = "Outline";
	info.description = "Add outline around any image or text.";
	info.has_audio = false;
	info.has_video = true;
}

// This method is required for all derived classes of EffectBase, and returns a
// modified openshot::Frame object
std::shared_ptr<openshot::Frame> Outline::GetFrame(std::shared_ptr<openshot::Frame> frame, int64_t frame_number)
{
	float widthValue = width.GetValue(frame_number);
	int blueValue = color.blue.GetValue(frame_number);
	int greenValue = color.green.GetValue(frame_number);
	int redValue = color.red.GetValue(frame_number);
	int alphaValue = color.alpha.GetValue(frame_number);
	
	if (widthValue <= 0.0 || alphaValue <= 0) {
		// If alpha or width is zero, return the original frame
		return frame;
	}

	// Get the frame's image
	cv::Mat cv_image = frame->GetBGRACvMat();
	
	float sigmaValue = widthValue / 3.0;
	if (sigmaValue <= 0.0)
		sigmaValue = 0.01;

	// Extract alpha channel for the mask
	std::vector<cv::Mat> channels(4);
	cv::split(cv_image, channels);
	cv::Mat alpha_mask = channels[3].clone();

	// Create the outline mask
	cv::Mat outline_mask;
	cv::GaussianBlur(alpha_mask, outline_mask, cv::Size(0, 0), sigmaValue, sigmaValue, cv::BorderTypes::BORDER_DEFAULT);
	cv::threshold(outline_mask, outline_mask, 0, 255, cv::ThresholdTypes::THRESH_BINARY);

	// Antialias the outline edge & apply Canny edge detection
	cv::Mat edge_mask;
	cv::Canny(outline_mask, edge_mask, 250, 255);

	// Apply Gaussian blur only to the edge mask
	cv::Mat blurred_edge_mask;
	cv::GaussianBlur(edge_mask, blurred_edge_mask, cv::Size(0, 0), 0.8, 0.8, cv::BorderTypes::BORDER_DEFAULT);
	cv::bitwise_or(outline_mask, blurred_edge_mask, outline_mask);

	cv::Mat final_image;

	// Create solid color source mat (cv::Scalar: red, green, blue, alpha)
	cv::Mat solid_color_mat(cv::Size(cv_image.cols, cv_image.rows), CV_8UC4, cv::Scalar(redValue, greenValue, blueValue, alphaValue));

	// Place outline first, then the original image on top
	solid_color_mat.copyTo(final_image, outline_mask);
	cv_image.copyTo(final_image, alpha_mask);

	frame->SetBGRACvMat(final_image);
	
	return frame;
}

// Moved to Frame.cpp
// cv::Mat Outline::QImageToBGRACvMat(std::shared_ptr<QImage>& qimage) {
// 	cv::Mat cv_img(qimage->height(), qimage->width(), CV_8UC4, (uchar*)qimage->constBits(), qimage->bytesPerLine());
// 	return cv_img;
// }

// std::shared_ptr<QImage> Outline::BGRACvMatToQImage(cv::Mat img) {
// 	cv::Mat final_img;
// 	cv::cvtColor(img, final_img, cv::COLOR_RGBA2BGRA);
// 	QImage qimage(final_img.data, final_img.cols, final_img.rows, final_img.step, QImage::Format_ARGB32);
// 	std::shared_ptr<QImage> imgIn = std::make_shared<QImage>(qimage.convertToFormat(QImage::Format_RGBA8888_Premultiplied));
// 	return imgIn;
// }

// Generate JSON string of this object
std::string Outline::Json() const {

	// Return formatted string
	return JsonValue().toStyledString();
}

// Generate Json::Value for this object
Json::Value Outline::JsonValue() const {

	// Create root json object
	Json::Value root = EffectBase::JsonValue(); // get parent properties
	root["type"] = info.class_name;
	root["width"] = width.JsonValue();
	root["color"] = color.JsonValue();

	// return JsonValue
	return root;
}

// Load JSON string into this object
void Outline::SetJson(const std::string value) {

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
void Outline::SetJsonValue(const Json::Value root) {

	// Set parent data
	EffectBase::SetJsonValue(root);

	// Set data from Json (if key is found)
	if (!root["width"].isNull())
		width.SetJsonValue(root["width"]);
	if (!root["color"].isNull())
		color.SetJsonValue(root["color"]);
}

// Get all properties for a specific frame
std::string Outline::PropertiesJSON(int64_t requested_frame) const {

	// Generate JSON properties list
	Json::Value root = BasePropertiesJSON(requested_frame);

	// Keyframes
	root["width"] = add_property_json("Width", width.GetValue(requested_frame), "float", "", &width, 0, 100, false, requested_frame);
	root["color"] = add_property_json("Key Color", 0.0, "color", "", &color.red, 0, 255, false, requested_frame);
	root["color"]["red"] = add_property_json("Red", color.red.GetValue(requested_frame), "float", "", &color.red, 0, 255, false, requested_frame);
	root["color"]["blue"] = add_property_json("Blue", color.blue.GetValue(requested_frame), "float", "", &color.blue, 0, 255, false, requested_frame);
	root["color"]["green"] = add_property_json("Green", color.green.GetValue(requested_frame), "float", "", &color.green, 0, 255, false, requested_frame);
	root["color"]["alpha"] = add_property_json("Alpha", color.alpha.GetValue(requested_frame), "float", "", &color.alpha, 0, 255, false, requested_frame);

	// Return formatted string
	return root.toStyledString();
}
