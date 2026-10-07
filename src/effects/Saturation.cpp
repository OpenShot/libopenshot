/**
 * @file
 * @brief Source file for Saturation class
 * @author Jonathan Thomas <jonathan@openshot.org>
 *
 * @ref License
 */

// Copyright (c) 2008-2019 OpenShot Studios, LLC
//
// SPDX-License-Identifier: LGPL-3.0-or-later

#include "Saturation.h"
#include "Exceptions.h"
#include <array>
#include <cmath>

using namespace openshot;

/// Blank constructor, useful when using Json to load the effect properties
Saturation::Saturation() : saturation(1.0), saturation_R(1.0), saturation_G(1.0), saturation_B(1.0),
	mask_mode(SATURATION_MASK_POST_BLEND) {
	// Init effect properties
	init_effect_details();
}

// Default constructor
Saturation::Saturation(Keyframe saturation, Keyframe saturation_R, Keyframe saturation_G, Keyframe saturation_B) :
		saturation(saturation), saturation_R(saturation_R), saturation_G(saturation_G), saturation_B(saturation_B),
		mask_mode(SATURATION_MASK_POST_BLEND)
{
	// Init effect properties
	init_effect_details();
}

bool Saturation::UseCustomMaskBlend(int64_t frame_number) const {
	(void) frame_number;
	return mask_mode == SATURATION_MASK_DRIVE_AMOUNT;
}

void Saturation::ApplyCustomMaskBlend(std::shared_ptr<QImage> original_image, std::shared_ptr<QImage> effected_image,
									  std::shared_ptr<QImage> mask_image, int64_t frame_number) const {
	(void) frame_number;
	if (!original_image || !effected_image || !mask_image)
		return;
	if (original_image->size() != effected_image->size() || effected_image->size() != mask_image->size())
		return;

	unsigned char* original_pixels = reinterpret_cast<unsigned char*>(original_image->bits());
	unsigned char* effected_pixels = reinterpret_cast<unsigned char*>(effected_image->bits());
	unsigned char* mask_pixels = reinterpret_cast<unsigned char*>(mask_image->bits());
	const int width = effected_image->width();
	const int height = effected_image->height();
	const int original_stride = original_image->bytesPerLine();
	const int effected_stride = effected_image->bytesPerLine();
	const int mask_stride = mask_image->bytesPerLine();

	if (mask_invert) {
		#pragma omp parallel for schedule(static)
		for (int y = 0; y < height; ++y) {
			unsigned char* original_row = original_pixels + y * original_stride;
			unsigned char* effected_row = effected_pixels + y * effected_stride;
			unsigned char* mask_row = mask_pixels + y * mask_stride;
			for (int x = 0; x < width; ++x) {
				const int idx = x * 4;
				float factor = static_cast<float>(qGray(mask_row[idx], mask_row[idx + 1], mask_row[idx + 2])) / 255.0f;
				factor = 1.0f - factor;
				// Use a non-linear response curve for custom saturation drive mode.
				factor = factor * factor;
				const float inverse = 1.0f - factor;

				// Drive saturation strength with mask while preserving source alpha.
				effected_row[idx] = static_cast<unsigned char>(
					(original_row[idx] * inverse) + (effected_row[idx] * factor));
				effected_row[idx + 1] = static_cast<unsigned char>(
					(original_row[idx + 1] * inverse) + (effected_row[idx + 1] * factor));
				effected_row[idx + 2] = static_cast<unsigned char>(
					(original_row[idx + 2] * inverse) + (effected_row[idx + 2] * factor));
				effected_row[idx + 3] = original_row[idx + 3];
			}
		}
	} else {
		#pragma omp parallel for schedule(static)
		for (int y = 0; y < height; ++y) {
			unsigned char* original_row = original_pixels + y * original_stride;
			unsigned char* effected_row = effected_pixels + y * effected_stride;
			unsigned char* mask_row = mask_pixels + y * mask_stride;
			for (int x = 0; x < width; ++x) {
				const int idx = x * 4;
				float factor = static_cast<float>(qGray(mask_row[idx], mask_row[idx + 1], mask_row[idx + 2])) / 255.0f;
				// Use a non-linear response curve for custom saturation drive mode.
				factor = factor * factor;
				const float inverse = 1.0f - factor;

				// Drive saturation strength with mask while preserving source alpha.
				effected_row[idx] = static_cast<unsigned char>(
					(original_row[idx] * inverse) + (effected_row[idx] * factor));
				effected_row[idx + 1] = static_cast<unsigned char>(
					(original_row[idx + 1] * inverse) + (effected_row[idx + 1] * factor));
				effected_row[idx + 2] = static_cast<unsigned char>(
					(original_row[idx + 2] * inverse) + (effected_row[idx + 2] * factor));
				effected_row[idx + 3] = original_row[idx + 3];
			}
		}
	}
}

// Init effect settings
void Saturation::init_effect_details()
{
	/// Initialize the values of the EffectInfo struct.
	InitEffectInfo();

	/// Set the effect info
	info.class_name = "Saturation";
	info.name = "Color Saturation";
	info.description = "Adjust the color saturation.";
	info.has_audio = false;
	info.has_video = true;
}

// This method is required for all derived classes of EffectBase, and returns a
// modified openshot::Frame object
std::shared_ptr<openshot::Frame> Saturation::GetFrame(std::shared_ptr<openshot::Frame> frame, int64_t frame_number)
{
	// Get the frame's image
	std::shared_ptr<QImage> frame_image = frame->GetImage();

	if (!frame_image)
		return frame;

	const int pixel_count = frame_image->width() * frame_image->height();

	// Get keyframe values for this frame
	const float saturation_value = saturation.GetValue(frame_number);
	const float saturation_value_R = saturation_R.GetValue(frame_number);
	const float saturation_value_G = saturation_G.GetValue(frame_number);
	const float saturation_value_B = saturation_B.GetValue(frame_number);

	// Constants used for color saturation formula
	const float pR = 0.299f;
	const float pG = 0.587f;
	const float pB = 0.114f;
	const float sqrt_pR = std::sqrt(pR);
	const float sqrt_pG = std::sqrt(pG);
	const float sqrt_pB = std::sqrt(pB);
	// Rec.601 fixed-point luma weights used in many image/video pipelines.
	// This avoids per-pixel sqrt() while keeping output stable.
	static const std::array<float, 65026> sqrt_lut = [] {
		std::array<float, 65026> lut{};
		for (int i = 0; i <= 65025; ++i)
			lut[i] = std::sqrt(static_cast<float>(i));
		return lut;
	}();

	// Loop through pixels
	unsigned char *pixels = reinterpret_cast<unsigned char *>(frame_image->bits());
	const int width = frame_image->width();
	const int height = frame_image->height();
	const int stride = frame_image->bytesPerLine();
	// LUT for undoing premultiplication without a per-pixel divide.
	static const std::array<float, 256> inv_alpha = [] {
		std::array<float, 256> lut{};
		lut[0] = 0.0f;
		for (int i = 1; i < 256; ++i)
			lut[i] = 255.0f / static_cast<float>(i);
		return lut;
	}();
	const auto clamp_i = [](int value) -> int {
		if (value < 0) return 0;
		if (value > 255) return 255;
		return value;
	};

	const auto apply_saturation = [&](int &R, int &G, int &B) {
		// Approximate sqrt(R^2*pR + G^2*pG + B^2*pB) with fixed-point weighted
		// intensity and lookup table. 77/150/29 ~= 0.299/0.587/0.114.
		const int weighted_sq = (77 * R * R + 150 * G * G + 29 * B * B + 128) >> 8;
		const float p = sqrt_lut[weighted_sq];

		// Adjust common saturation
		R = clamp_i(static_cast<int>(p + (R - p) * saturation_value));
		G = clamp_i(static_cast<int>(p + (G - p) * saturation_value));
		B = clamp_i(static_cast<int>(p + (B - p) * saturation_value));

		// Compute per-channel replacement brightness
		const float p_r = R * sqrt_pR;
		const float p_g = G * sqrt_pG;
		const float p_b = B * sqrt_pB;

		// Adjust channel-separated saturation
		const int Rr = static_cast<int>(p_r + (R - p_r) * saturation_value_R);
		const int Gr = static_cast<int>(p_r - p_r * saturation_value_R);
		const int Br = static_cast<int>(p_r - p_r * saturation_value_R);

		const int Rg = static_cast<int>(p_g - p_g * saturation_value_G);
		const int Gg = static_cast<int>(p_g + (G - p_g) * saturation_value_G);
		const int Bg = static_cast<int>(p_g - p_g * saturation_value_G);

		const int Rb = static_cast<int>(p_b - p_b * saturation_value_B);
		const int Gb = static_cast<int>(p_b - p_b * saturation_value_B);
		const int Bb = static_cast<int>(p_b + (B - p_b) * saturation_value_B);

		// Recombine and constrain values
		R = clamp_i(Rr + Rg + Rb);
		G = clamp_i(Gr + Gg + Gb);
		B = clamp_i(Br + Bg + Bb);
	};

	#pragma omp parallel for if(pixel_count >= 16384) schedule(static) shared (pixels)
	for (int y = 0; y < height; ++y)
	{
		unsigned char* row = pixels + y * stride;
		for (int x = 0; x < width; ++x) {
			const int idx = x * 4;

			// Split hot paths by alpha to avoid unnecessary premultiply/unpremultiply work.
			const int A = row[idx + 3];
			if (A <= 0)
				continue;
			int R = 0;
			int G = 0;
			int B = 0;
			if (A == 255) {
				R = row[idx + 0];
				G = row[idx + 1];
				B = row[idx + 2];
				apply_saturation(R, G, B);
				row[idx + 0] = static_cast<unsigned char>(R);
				row[idx + 1] = static_cast<unsigned char>(G);
				row[idx + 2] = static_cast<unsigned char>(B);
			} else {
				const float alpha_percent = static_cast<float>(A) * (1.0f / 255.0f);
				const float inv_alpha_percent = inv_alpha[A];

				// Get RGB values, and remove pre-multiplied alpha
				R = static_cast<int>(row[idx + 0] * inv_alpha_percent);
				G = static_cast<int>(row[idx + 1] * inv_alpha_percent);
				B = static_cast<int>(row[idx + 2] * inv_alpha_percent);
				apply_saturation(R, G, B);

				// Pre-multiply alpha back into color channels
				row[idx + 0] = static_cast<unsigned char>(R * alpha_percent);
				row[idx + 1] = static_cast<unsigned char>(G * alpha_percent);
				row[idx + 2] = static_cast<unsigned char>(B * alpha_percent);
			}
		}
	}

	// return the modified frame
	return frame;
}

// Generate JSON string of this object
std::string Saturation::Json() const {

	// Return formatted string
	return JsonValue().toStyledString();
}

// Generate Json::Value for this object
Json::Value Saturation::JsonValue() const {

	// Create root json object
	Json::Value root = EffectBase::JsonValue(); // get parent properties
	root["type"] = info.class_name;
	root["saturation"] = saturation.JsonValue();
	root["saturation_R"] = saturation_R.JsonValue();
	root["saturation_G"] = saturation_G.JsonValue();
	root["saturation_B"] = saturation_B.JsonValue();
	root["mask_mode"] = mask_mode;

	// return JsonValue
	return root;
}

// Load JSON string into this object
void Saturation::SetJson(const std::string value) {

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
void Saturation::SetJsonValue(const Json::Value root) {

	// Set parent data
	EffectBase::SetJsonValue(root);

	// Set data from Json (if key is found)
	if (!root["saturation"].isNull())
		saturation.SetJsonValue(root["saturation"]);
	if (!root["saturation_R"].isNull())
		saturation_R.SetJsonValue(root["saturation_R"]);
	if (!root["saturation_G"].isNull())
		saturation_G.SetJsonValue(root["saturation_G"]);
	if (!root["saturation_B"].isNull())
		saturation_B.SetJsonValue(root["saturation_B"]);
	if (!root["mask_mode"].isNull())
		mask_mode = root["mask_mode"].asInt();
}

// Get all properties for a specific frame
std::string Saturation::PropertiesJSON(int64_t requested_frame) const {

	// Generate JSON properties list
	Json::Value root = BasePropertiesJSON(requested_frame);

	// Keyframes
	root["saturation"] = add_property_json("Saturation", saturation.GetValue(requested_frame), "float", "", &saturation, 0.0, 4.0, false, requested_frame);
	root["saturation_R"] = add_property_json("Saturation (Red)", saturation_R.GetValue(requested_frame), "float", "", &saturation_R, 0.0, 4.0, false, requested_frame);
	root["saturation_G"] = add_property_json("Saturation (Green)", saturation_G.GetValue(requested_frame), "float", "", &saturation_G, 0.0, 4.0, false, requested_frame);
	root["saturation_B"] = add_property_json("Saturation (Blue)", saturation_B.GetValue(requested_frame), "float", "", &saturation_B, 0.0, 4.0, false, requested_frame);
	root["mask_mode"] = add_property_json("Mask Mode", mask_mode, "int", "", NULL, 0, 1, false, requested_frame);
	root["mask_mode"]["choices"].append(add_property_choice_json("Limit to Mask", SATURATION_MASK_POST_BLEND, mask_mode));
	root["mask_mode"]["choices"].append(add_property_choice_json("Vary Strength", SATURATION_MASK_DRIVE_AMOUNT, mask_mode));

	// Return formatted string
	return root.toStyledString();
}
