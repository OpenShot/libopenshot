/**
 * @file
 * @brief Source file for Mask class
 * @author Jonathan Thomas <jonathan@openshot.org>
 *
 * @ref License
 */

// Copyright (c) 2008-2019 OpenShot Studios, LLC
//
// SPDX-License-Identifier: LGPL-3.0-or-later

#include "Mask.h"

#include "Exceptions.h"

#include "ReaderBase.h"
#include <array>
#include <omp.h>

using namespace openshot;

/// Blank constructor, useful when using Json to load the effect properties
Mask::Mask() : replace_image(false), fade_audio_hint(false) {
	// Init effect properties
	init_effect_details();
}

// Default constructor
Mask::Mask(ReaderBase *mask_reader, Keyframe mask_brightness, Keyframe mask_contrast) :
		brightness(mask_brightness), contrast(mask_contrast), replace_image(false), fade_audio_hint(false)
{
	// Init effect properties
	init_effect_details();

	// Keep ownership local by cloning externally-provided readers.
	if (mask_reader)
		MaskReader(CreateReaderFromJson(mask_reader->JsonValue()));
}

// Init effect settings
void Mask::init_effect_details()
{
	/// Initialize the values of the EffectInfo struct.
	InitEffectInfo();

	/// Set the effect info
	info.class_name = "Mask";
	info.name = "Alpha Mask / Wipe Transition";
	info.description = "Uses a grayscale mask image to gradually wipe / transition between 2 images.";
	info.has_audio = false;
	info.has_video = true;
}

// This method is required for all derived classes of EffectBase, and returns a
// modified openshot::Frame object
std::shared_ptr<openshot::Frame> Mask::GetFrame(std::shared_ptr<openshot::Frame> frame, int64_t frame_number) {
	// Get frame image first
	std::shared_ptr<QImage> frame_image = frame->GetImage();
	if (!frame_image || frame_image->isNull())
		return frame;

	// No reader (bail on applying the mask)
	auto original_mask = ResolveMaskImage(frame_image, frame_number);
	if (!original_mask || original_mask->isNull())
		return frame;

	// Grab raw pointers and dimensions one time
	unsigned char* pixels      = reinterpret_cast<unsigned char*>(frame_image->bits());
	unsigned char* mask_pixels = reinterpret_cast<unsigned char*>(original_mask->bits());
	const int num_pixels        = original_mask->width() * original_mask->height();

	// Evaluate brightness and contrast keyframes just once
	double contrast_value   = contrast.GetValue(frame_number);
	double brightness_value = brightness.GetValue(frame_number);

	int brightness_adj = static_cast<int>(255 * brightness_value);
	float contrast_factor = 20.0f / std::max(0.5f, 20.0f - static_cast<float>(contrast_value));
	const bool output_mask = replace_image;
	const auto clamp_u8 = [](int value) -> unsigned char {
		if (value < 0) return 0;
		if (value > 255) return 255;
		return static_cast<unsigned char>(value);
	};
	// Precompute gray->adjusted-gray mapping for this frame's brightness/contrast.
	std::array<unsigned char, 256> adjusted_gray{};
	for (int gray = 0; gray < 256; ++gray) {
		const int adjusted = static_cast<int>(contrast_factor * ((gray + brightness_adj) - 128) + 128);
		adjusted_gray[gray] = clamp_u8(adjusted);
	}
	// 8-bit multiply lookup for premultiplied alpha channel scaling.
	static const std::array<std::array<unsigned char, 256>, 256> mul_lut = [] {
		std::array<std::array<unsigned char, 256>, 256> lut{};
		for (int alpha = 0; alpha < 256; ++alpha) {
			for (int value = 0; value < 256; ++value) {
				lut[alpha][value] = static_cast<unsigned char>((value * alpha) / 255);
			}
		}
		return lut;
	}();

	// Separate loops keep the hot path branch-free per pixel.
	if (output_mask) {
		#pragma omp parallel for if(num_pixels >= 16384) schedule(static)
		for (int i = 0; i < num_pixels; ++i) {
			const int idx = i * 4;
			const int R = mask_pixels[idx + 0];
			const int G = mask_pixels[idx + 1];
			const int B = mask_pixels[idx + 2];
			const int A = mask_pixels[idx + 3];

			const int gray = ((R * 11) + (G * 16) + (B * 5)) >> 5;
			const int diff = A - adjusted_gray[gray];
			const unsigned char new_val = clamp_u8(diff);
			pixels[idx + 0] = new_val;
			pixels[idx + 1] = new_val;
			pixels[idx + 2] = new_val;
			pixels[idx + 3] = new_val;
		}
	} else if (mask_invert) {
		#pragma omp parallel for if(num_pixels >= 16384) schedule(static)
		for (int i = 0; i < num_pixels; ++i) {
			const int idx = i * 4;
			const int R = mask_pixels[idx + 0];
			const int G = mask_pixels[idx + 1];
			const int B = mask_pixels[idx + 2];
			const int A = mask_pixels[idx + 3];

			const int gray = ((R * 11) + (G * 16) + (B * 5)) >> 5;
			int alpha = A - adjusted_gray[gray];
			if (alpha < 0) alpha = 0;
			else if (alpha > 255) alpha = 255;
			alpha = 255 - alpha;

			// Premultiplied RGBA → multiply each channel by alpha
			pixels[idx + 0] = mul_lut[alpha][pixels[idx + 0]];
			pixels[idx + 1] = mul_lut[alpha][pixels[idx + 1]];
			pixels[idx + 2] = mul_lut[alpha][pixels[idx + 2]];
			pixels[idx + 3] = mul_lut[alpha][pixels[idx + 3]];
		}
	} else {
		#pragma omp parallel for if(num_pixels >= 16384) schedule(static)
		for (int i = 0; i < num_pixels; ++i) {
			const int idx = i * 4;
			const int R = mask_pixels[idx + 0];
			const int G = mask_pixels[idx + 1];
			const int B = mask_pixels[idx + 2];
			const int A = mask_pixels[idx + 3];

			const int gray = ((R * 11) + (G * 16) + (B * 5)) >> 5;
			int alpha = A - adjusted_gray[gray];
			if (alpha < 0) alpha = 0;
			else if (alpha > 255) alpha = 255;

			// Premultiplied RGBA → multiply each channel by alpha
			pixels[idx + 0] = mul_lut[alpha][pixels[idx + 0]];
			pixels[idx + 1] = mul_lut[alpha][pixels[idx + 1]];
			pixels[idx + 2] = mul_lut[alpha][pixels[idx + 2]];
			pixels[idx + 3] = mul_lut[alpha][pixels[idx + 3]];
		}
	}

	// return the modified frame
	return frame;
}

// Generate JSON string of this object
std::string Mask::Json() const {

	// Return formatted string
	return JsonValue().toStyledString();
}

// Generate Json::Value for this object
Json::Value Mask::JsonValue() const {

	// Create root json object
	Json::Value root = EffectBase::JsonValue(); // get parent properties
	root["type"] = info.class_name;
	root["brightness"] = brightness.JsonValue();
	root["contrast"] = contrast.JsonValue();
	root["replace_image"] = replace_image;
	root["fade_audio_hint"] = fade_audio_hint;

	// return JsonValue
	return root;
}

// Load JSON string into this object
void Mask::SetJson(const std::string value) {

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
void Mask::SetJsonValue(const Json::Value root) {
	Json::Value normalized_root = root;
	// Legacy compatibility: keep accepting "reader" on Mask effects.
	if (!normalized_root["reader"].isNull() && normalized_root["mask_reader"].isNull())
		normalized_root["mask_reader"] = normalized_root["reader"];

	// Set parent data
	EffectBase::SetJsonValue(normalized_root);

	// Set data from Json (if key is found)
	if (!normalized_root["replace_image"].isNull())
		replace_image = normalized_root["replace_image"].asBool();
	if (!normalized_root["fade_audio_hint"].isNull())
		fade_audio_hint = normalized_root["fade_audio_hint"].asBool();
	if (!normalized_root["brightness"].isNull())
		brightness.SetJsonValue(normalized_root["brightness"]);
	if (!normalized_root["contrast"].isNull())
		contrast.SetJsonValue(normalized_root["contrast"]);

}

// Get all properties for a specific frame
std::string Mask::PropertiesJSON(int64_t requested_frame) const {

	// Generate JSON properties list
	Json::Value root = BasePropertiesJSON(requested_frame);

	// Add replace_image choices (dropdown style)
	root["replace_image"] = add_property_json("Replace Image", replace_image, "int", "", NULL, 0, 1, false, requested_frame);
	root["replace_image"]["choices"].append(add_property_choice_json("Yes", true, replace_image));
	root["replace_image"]["choices"].append(add_property_choice_json("No", false, replace_image));

	root["fade_audio_hint"] = add_property_json("Fade Audio", fade_audio_hint, "int", "", NULL, 0, 1, false, requested_frame);
	root["fade_audio_hint"]["choices"].append(add_property_choice_json("Yes", true, fade_audio_hint));
	root["fade_audio_hint"]["choices"].append(add_property_choice_json("No", false, fade_audio_hint));

	// Keyframes
	root["brightness"] = add_property_json("Brightness", brightness.GetValue(requested_frame), "float", "", &brightness, -1.0, 1.0, false, requested_frame);
	root["contrast"] = add_property_json("Contrast", contrast.GetValue(requested_frame), "float", "", &contrast, 0, 20, false, requested_frame);

	// Return formatted string
	return root.toStyledString();
}

void Mask::Reader(ReaderBase *new_reader) {
	if (!new_reader) {
		MaskReader(NULL);
		return;
	}

	// Keep ownership local by cloning externally-provided readers.
	MaskReader(CreateReaderFromJson(new_reader->JsonValue()));
}
