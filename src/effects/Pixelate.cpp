/**
 * @file
 * @brief Source file for Pixelate effect class
 * @author Jonathan Thomas <jonathan@openshot.org>
 *
 * @ref License
 */

// Copyright (c) 2008-2019 OpenShot Studios, LLC
//
// SPDX-License-Identifier: LGPL-3.0-or-later

#include "Pixelate.h"
#include "Exceptions.h"
#include "Json.h"

#include <QImage>
#include <QPainter>
#include <QRect>
#include <QPoint>

#include <algorithm>

using namespace openshot;

namespace {
double clamp_margin(double value) {
	if (value < 0.0)
		return 0.0;
	if (value > 1.0)
		return 1.0;
	return value;
}
}

/// Blank constructor, useful when using Json to load the effect properties
Pixelate::Pixelate() : pixelization(0.5), left(0.0), top(0.0), right(0.0), bottom(0.0),
	mask_mode(PIXELATE_MASK_LIMIT_TO_AREA) {
	// Init effect properties
	init_effect_details();
}

// Default constructor
Pixelate::Pixelate(Keyframe pixelization, Keyframe left, Keyframe top, Keyframe right, Keyframe bottom) :
	pixelization(pixelization), left(left), top(top), right(right), bottom(bottom),
	mask_mode(PIXELATE_MASK_LIMIT_TO_AREA)
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
std::shared_ptr<openshot::Frame>
Pixelate::GetFrame(std::shared_ptr<openshot::Frame> frame, int64_t frame_number)
{
	// Get the frame's image
	std::shared_ptr<QImage> frame_image = frame->GetImage();

	// Get current keyframe values
	double pixelization_value = std::min(pow(0.001, fabs(pixelization.GetValue(frame_number))), 1.0);
	double left_value = left.GetValue(frame_number);
	double top_value = top.GetValue(frame_number);
	double right_value = right.GetValue(frame_number);
	double bottom_value = bottom.GetValue(frame_number);

	if (pixelization_value > 0.0) {
		int w = frame_image->width();
		int h = frame_image->height();
		if (w <= 0 || h <= 0)
			return frame;

		// Define area we're working on in terms of a QRect with QMargins applied
		QRect area(QPoint(0,0), frame_image->size());
		area = area.marginsRemoved({
			int(clamp_margin(left_value) * w),
			int(clamp_margin(top_value) * h),
			int(clamp_margin(right_value) * w),
			int(clamp_margin(bottom_value) * h)
		});
		area = area.intersected(QRect(QPoint(0,0), frame_image->size()));
		if (area.isEmpty())
			return frame;

		int scale_to = (int) (area.width() * pixelization_value);
		if (scale_to < 1) {
			scale_to = 1; // Not less than one pixel
		}
		// Copy and scale pixels in area to be pixelated
		auto frame_scaled = frame_image->copy(area).scaledToWidth(scale_to, Qt::SmoothTransformation);

		// Draw pixelated image back over original
		QPainter painter(frame_image.get());
		painter.drawImage(area, frame_scaled);
		painter.end();
	}

	// return the modified frame
	return frame;
}

bool Pixelate::UseCustomMaskBlend(int64_t frame_number) const {
	(void) frame_number;
	return mask_mode == PIXELATE_MASK_VARY_STRENGTH;
}

void Pixelate::ApplyCustomMaskBlend(std::shared_ptr<QImage> original_image, std::shared_ptr<QImage> effected_image,
									std::shared_ptr<QImage> mask_image, int64_t frame_number) const {
	(void) frame_number;
	if (!original_image || !effected_image || !mask_image)
		return;
	if (original_image->size() != effected_image->size() || effected_image->size() != mask_image->size())
		return;

	unsigned char* original_pixels = reinterpret_cast<unsigned char*>(original_image->bits());
	unsigned char* effected_pixels = reinterpret_cast<unsigned char*>(effected_image->bits());
	unsigned char* mask_pixels = reinterpret_cast<unsigned char*>(mask_image->bits());
	const int pixel_count = effected_image->width() * effected_image->height();

	#pragma omp parallel for schedule(static)
	for (int i = 0; i < pixel_count; ++i) {
		const int idx = i * 4;
		float factor = static_cast<float>(qGray(mask_pixels[idx], mask_pixels[idx + 1], mask_pixels[idx + 2])) / 255.0f;
		if (mask_invert)
			factor = 1.0f - factor;
		factor = factor * factor;
		const float inverse = 1.0f - factor;

		effected_pixels[idx] = static_cast<unsigned char>(
			(original_pixels[idx] * inverse) + (effected_pixels[idx] * factor));
		effected_pixels[idx + 1] = static_cast<unsigned char>(
			(original_pixels[idx + 1] * inverse) + (effected_pixels[idx + 1] * factor));
		effected_pixels[idx + 2] = static_cast<unsigned char>(
			(original_pixels[idx + 2] * inverse) + (effected_pixels[idx + 2] * factor));
		effected_pixels[idx + 3] = original_pixels[idx + 3];
	}
}

// Generate JSON string of this object
std::string Pixelate::Json() const {

	// Return formatted string
	return JsonValue().toStyledString();
}

// Generate Json::Value for this object
Json::Value Pixelate::JsonValue() const {

	// Create root json object
	Json::Value root = EffectBase::JsonValue(); // get parent properties
	root["type"] = info.class_name;
	root["pixelization"] = pixelization.JsonValue();
	root["left"] = left.JsonValue();
	root["top"] = top.JsonValue();
	root["right"] = right.JsonValue();
	root["bottom"] = bottom.JsonValue();
	root["mask_mode"] = mask_mode;

	// return JsonValue
	return root;
}

// Load JSON string into this object
void Pixelate::SetJson(const std::string value) {

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
void Pixelate::SetJsonValue(const Json::Value root) {

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
	if (!root["mask_mode"].isNull())
		mask_mode = root["mask_mode"].asInt();
}

// Get all properties for a specific frame
std::string Pixelate::PropertiesJSON(int64_t requested_frame) const {

	// Generate JSON properties list
	Json::Value root = BasePropertiesJSON(requested_frame);

	// Keyframes
	root["pixelization"] = add_property_json("Pixelization", pixelization.GetValue(requested_frame), "float", "", &pixelization, 0.0, 0.9999, false, requested_frame);
	root["left"] = add_property_json("Margin: Left", left.GetValue(requested_frame), "float", "", &left, 0.0, 1.0, false, requested_frame);
	root["top"] = add_property_json("Margin: Top", top.GetValue(requested_frame), "float", "", &top, 0.0, 1.0, false, requested_frame);
	root["right"] = add_property_json("Margin: Right", right.GetValue(requested_frame), "float", "", &right, 0.0, 1.0, false, requested_frame);
	root["bottom"] = add_property_json("Margin: Bottom", bottom.GetValue(requested_frame), "float", "", &bottom, 0.0, 1.0, false, requested_frame);
	root["mask_mode"] = add_property_json("Mask Mode", mask_mode, "int", "", NULL, 0, 1, false, requested_frame);
	root["mask_mode"]["choices"].append(add_property_choice_json("Limit to Mask", PIXELATE_MASK_LIMIT_TO_AREA, mask_mode));
	root["mask_mode"]["choices"].append(add_property_choice_json("Vary Strength", PIXELATE_MASK_VARY_STRENGTH, mask_mode));

	// Return formatted string
	return root.toStyledString();
}
