/**
 * @file
 * @brief Source file for Blur effect class
 * @author Jonathan Thomas <jonathan@openshot.org>
 *
 * @ref License
 */

// Copyright (c) 2008-2019 OpenShot Studios, LLC
//
// SPDX-License-Identifier: LGPL-3.0-or-later

#include "Blur.h"
#include "Exceptions.h"

#include <QMargins>
#include <QPainter>
#include <QPoint>
#include <QRect>

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
Blur::Blur() : horizontal_radius(6.0), vertical_radius(6.0), sigma(3.0), iterations(3.0),
	left(0.0), top(0.0), right(0.0), bottom(0.0),
	mask_mode(BLUR_MASK_POST_BLEND) {
	// Init effect properties
	init_effect_details();
}

// Default constructor
Blur::Blur(Keyframe new_horizontal_radius, Keyframe new_vertical_radius, Keyframe new_sigma, Keyframe new_iterations) :
		horizontal_radius(new_horizontal_radius), vertical_radius(new_vertical_radius),
		sigma(new_sigma), iterations(new_iterations),
		left(0.0), top(0.0), right(0.0), bottom(0.0),
		mask_mode(BLUR_MASK_POST_BLEND)
{
	// Init effect properties
	init_effect_details();
}

// Default constructor
Blur::Blur(Keyframe new_horizontal_radius, Keyframe new_vertical_radius, Keyframe new_sigma, Keyframe new_iterations,
		   Keyframe new_left, Keyframe new_top, Keyframe new_right, Keyframe new_bottom) :
		horizontal_radius(new_horizontal_radius), vertical_radius(new_vertical_radius),
		sigma(new_sigma), iterations(new_iterations),
		left(new_left), top(new_top), right(new_right), bottom(new_bottom),
		mask_mode(BLUR_MASK_POST_BLEND)
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
std::shared_ptr<openshot::Frame> Blur::GetFrame(std::shared_ptr<openshot::Frame> frame, int64_t frame_number)
{
	// Get the frame's image
	std::shared_ptr<QImage> frame_image = frame->GetImage();

	// Get the current blur radius
	int horizontal_radius_value = horizontal_radius.GetValue(frame_number);
	int vertical_radius_value = vertical_radius.GetValue(frame_number);
	float sigma_value = sigma.GetValue(frame_number);
	int iteration_value = iterations.GetInt(frame_number);
	(void) sigma_value;

	int w = frame_image->width();
	int h = frame_image->height();
	if (w <= 0 || h <= 0 || iteration_value <= 0)
		return frame;

	// Define area we're working on in terms of a QRect with QMargins applied.
	QRect area(QPoint(0, 0), frame_image->size());
	area = area.marginsRemoved({
		int(clamp_margin(left.GetValue(frame_number)) * w),
		int(clamp_margin(top.GetValue(frame_number)) * h),
		int(clamp_margin(right.GetValue(frame_number)) * w),
		int(clamp_margin(bottom.GetValue(frame_number)) * h)
	});
	area = area.intersected(QRect(QPoint(0, 0), frame_image->size()));
	if (area.isEmpty())
		return frame;

	// Grab two copies of the image pixel data
	QImage image_copy = frame_image->copy(area);
	std::shared_ptr<QImage> blur_image = std::make_shared<QImage>(image_copy);
	std::shared_ptr<QImage> frame_image_2 = std::make_shared<QImage>(image_copy);
	const int area_w = blur_image->width();
	const int area_h = blur_image->height();
	const int horizontal_area_radius = std::min(std::max(0, horizontal_radius_value), std::max(0, area_w - 1));
	const int vertical_area_radius = std::min(std::max(0, vertical_radius_value), std::max(0, area_h - 1));
	bool blurred = false;

	// Loop through each iteration
	for (int iteration = 0; iteration < iteration_value; ++iteration)
	{
		// HORIZONTAL BLUR (if any)
		if (horizontal_area_radius > 0.0) {
			// Apply horizontal blur to target RGBA channels
			boxBlurH(blur_image->bits(), frame_image_2->bits(), area_w, area_h, horizontal_area_radius);

			// Swap output image back to input
			blur_image.swap(frame_image_2);
			blurred = true;
		}

		// VERTICAL BLUR (if any)
		if (vertical_area_radius > 0.0) {
			// Apply vertical blur to target RGBA channels
			boxBlurT(blur_image->bits(), frame_image_2->bits(), area_w, area_h, vertical_area_radius);

			// Swap output image back to input
			blur_image.swap(frame_image_2);
			blurred = true;
		}
	}

	if (blurred) {
		QPainter painter(frame_image.get());
		painter.drawImage(area, *blur_image);
		painter.end();
	}

	// return the modified frame
	return frame;
}

bool Blur::UseCustomMaskBlend(int64_t frame_number) const {
	(void) frame_number;
	return mask_mode == BLUR_MASK_DRIVE_AMOUNT;
}

void Blur::ApplyCustomMaskBlend(std::shared_ptr<QImage> original_image, std::shared_ptr<QImage> effected_image,
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
		// Use a non-linear response curve for custom blur drive mode.
		factor = factor * factor;
		const float inverse = 1.0f - factor;

		// Drive blur amount with the grayscale mask while preserving source alpha.
		effected_pixels[idx] = static_cast<unsigned char>(
			(original_pixels[idx] * inverse) + (effected_pixels[idx] * factor));
		effected_pixels[idx + 1] = static_cast<unsigned char>(
			(original_pixels[idx + 1] * inverse) + (effected_pixels[idx + 1] * factor));
		effected_pixels[idx + 2] = static_cast<unsigned char>(
			(original_pixels[idx + 2] * inverse) + (effected_pixels[idx + 2] * factor));
		effected_pixels[idx + 3] = original_pixels[idx + 3];
	}
}

// Credit: http://blog.ivank.net/fastest-gaussian-blur.html (MIT License)
// Modified to process all four channels in a pixel array
void Blur::boxBlurH(unsigned char *scl, unsigned char *tcl, int w, int h, int r) {
	const float iarr = 1.0f / (r + r + 1);

	#pragma omp parallel for shared(scl, tcl)
	for (int i = 0; i < h; ++i) {
		const unsigned char* src = scl + i * w * 4;
		unsigned char* dst = tcl + i * w * 4;

		int val[4] = {0, 0, 0, 0};
		for (int j = -r; j <= r; ++j) {
			const int sample_x = std::min(std::max(j, 0), w - 1);
			const unsigned char* p = src + sample_x * 4;
			for (int c = 0; c < 4; ++c)
				val[c] += p[c];
		}

		for (int j = 0; j < w; ++j) {
			unsigned char* out = dst + j * 4;
			for (int c = 0; c < 4; ++c) {
				out[c] = (unsigned char)(val[c] * iarr + 0.5f);
			}

			const int remove_x = std::min(std::max(j - r, 0), w - 1);
			const int add_x = std::min(std::max(j + r + 1, 0), w - 1);
			const unsigned char* remove = src + remove_x * 4;
			const unsigned char* add = src + add_x * 4;
			for (int c = 0; c < 4; ++c) {
				val[c] += add[c] - remove[c];
			}
		}
	}
}

void Blur::boxBlurT(unsigned char *scl, unsigned char *tcl, int w, int h, int r) {
	const float iarr = 1.0f / (r + r + 1);
	const int stride = w * 4;

	#pragma omp parallel for shared(scl, tcl)
	for (int i = 0; i < w; ++i) {
		const unsigned char* col_src = scl + i * 4;
		unsigned char* col_dst = tcl + i * 4;

		int val[4] = {0, 0, 0, 0};
		for (int j = -r; j <= r; ++j) {
			const int sample_y = std::min(std::max(j, 0), h - 1);
			const unsigned char* p = col_src + sample_y * stride;
			for (int c = 0; c < 4; ++c)
				val[c] += p[c];
		}

		for (int j = 0; j < h; ++j) {
			unsigned char* out = col_dst + j * stride;
			for (int c = 0; c < 4; ++c) {
				out[c] = (unsigned char)(val[c] * iarr + 0.5f);
			}

			const int remove_y = std::min(std::max(j - r, 0), h - 1);
			const int add_y = std::min(std::max(j + r + 1, 0), h - 1);
			const unsigned char* remove = col_src + remove_y * stride;
			const unsigned char* add = col_src + add_y * stride;
			for (int c = 0; c < 4; ++c) {
				val[c] += add[c] - remove[c];
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
	root["left"] = left.JsonValue();
	root["top"] = top.JsonValue();
	root["right"] = right.JsonValue();
	root["bottom"] = bottom.JsonValue();
	root["mask_mode"] = mask_mode;

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
std::string Blur::PropertiesJSON(int64_t requested_frame) const {

	// Generate JSON properties list
	Json::Value root = BasePropertiesJSON(requested_frame);

	// Keyframes
	root["horizontal_radius"] = add_property_json("Horizontal Radius", horizontal_radius.GetValue(requested_frame), "float", "", &horizontal_radius, 0, 100, false, requested_frame);
	root["vertical_radius"] = add_property_json("Vertical Radius", vertical_radius.GetValue(requested_frame), "float", "", &vertical_radius, 0, 100, false, requested_frame);
	root["sigma"] = add_property_json("Sigma", sigma.GetValue(requested_frame), "float", "", &sigma, 0, 100, false, requested_frame);
	root["iterations"] = add_property_json("Iterations", iterations.GetValue(requested_frame), "float", "", &iterations, 0, 100, false, requested_frame);
	root["left"] = add_property_json("Margin: Left", left.GetValue(requested_frame), "float", "", &left, 0.0, 1.0, false, requested_frame);
	root["top"] = add_property_json("Margin: Top", top.GetValue(requested_frame), "float", "", &top, 0.0, 1.0, false, requested_frame);
	root["right"] = add_property_json("Margin: Right", right.GetValue(requested_frame), "float", "", &right, 0.0, 1.0, false, requested_frame);
	root["bottom"] = add_property_json("Margin: Bottom", bottom.GetValue(requested_frame), "float", "", &bottom, 0.0, 1.0, false, requested_frame);
	root["mask_mode"] = add_property_json("Mask Mode", mask_mode, "int", "", NULL, 0, 1, false, requested_frame);
	root["mask_mode"]["choices"].append(add_property_choice_json("Limit to Mask", BLUR_MASK_POST_BLEND, mask_mode));
	root["mask_mode"]["choices"].append(add_property_choice_json("Vary Strength", BLUR_MASK_DRIVE_AMOUNT, mask_mode));

	// Return formatted string
	return root.toStyledString();
}
