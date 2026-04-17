/**
 * @file
 * @brief Source file for Displace class
 * @author Jonathan Thomas <jonathan@openshot.org>
 *
 * @ref License
 */

// Copyright (c) 2008-2026 OpenShot Studios, LLC
//
// SPDX-License-Identifier: LGPL-3.0-or-later

#include "Displace.h"

#include "Exceptions.h"
#include "ReaderBase.h"
#include "Timeline.h"
#include "ZmqLogger.h"

#include <array>
#include <cmath>
#include <omp.h>

using namespace openshot;

namespace {
	inline unsigned char clamp_u8(int value) {
		if (value < 0)
			return 0;
		if (value > 255)
			return 255;
		return static_cast<unsigned char>(value);
	}

	inline int clamp_i(int value, int min_value, int max_value) {
		if (value < min_value)
			return min_value;
		if (value > max_value)
			return max_value;
		return value;
	}

	inline float sample_bilinear(const unsigned char* pixels, int width, int height, float x, float y, int channel) {
		x = std::fmax(0.0f, std::fmin(x, static_cast<float>(width - 1)));
		y = std::fmax(0.0f, std::fmin(y, static_cast<float>(height - 1)));

		const int x0 = static_cast<int>(std::floor(x));
		const int y0 = static_cast<int>(std::floor(y));
		const int x1 = clamp_i(x0 + 1, 0, width - 1);
		const int y1 = clamp_i(y0 + 1, 0, height - 1);
		const float tx = x - x0;
		const float ty = y - y0;

		const int idx00 = ((y0 * width) + x0) * 4 + channel;
		const int idx10 = ((y0 * width) + x1) * 4 + channel;
		const int idx01 = ((y1 * width) + x0) * 4 + channel;
		const int idx11 = ((y1 * width) + x1) * 4 + channel;

		const float top = (pixels[idx00] * (1.0f - tx)) + (pixels[idx10] * tx);
		const float bottom = (pixels[idx01] * (1.0f - tx)) + (pixels[idx11] * tx);
		return (top * (1.0f - ty)) + (bottom * ty);
	}
}

/// Blank constructor, useful when using Json to load the effect properties
Displace::Displace()
	: replace_image(false), invert(false), strength(1.0), horizontal(0.05), vertical(0.0), brightness(0.0), contrast(0.0) {
	init_effect_details();
}

// Default constructor
Displace::Displace(ReaderBase *map_reader, Keyframe map_strength, Keyframe map_horizontal,
				   Keyframe map_vertical, Keyframe map_brightness, Keyframe map_contrast)
	: replace_image(false), invert(false), strength(map_strength), horizontal(map_horizontal), vertical(map_vertical),
	  brightness(map_brightness), contrast(map_contrast) {
	init_effect_details();

	if (map_reader)
		Reader(CreateReaderFromJson(map_reader->JsonValue()));
}

// Init effect settings
void Displace::init_effect_details()
{
	InitEffectInfo();

	info.class_name = "Displace";
	info.name = "Displacement Map";
	info.description = "Use a grayscale image or video to warp the frame in the horizontal and vertical directions.";
	info.has_audio = false;
	info.has_video = true;
}

std::shared_ptr<QImage> Displace::GetMapImage(std::shared_ptr<QImage> target_image, int64_t frame_number) {
	if (!map_reader || !target_image || target_image->isNull())
		return {};

	std::shared_ptr<QImage> source_map;
	bool used_cached_scaled = false;
	#pragma omp critical (open_effect_displace_reader)
	{
		try {
			map_reader->ParentClip(ParentClip());
			if (!map_reader->IsOpen())
				map_reader->Open();

			if (map_reader->info.has_single_image &&
				cached_single_map_image &&
				cached_single_map_width == target_image->width() &&
				cached_single_map_height == target_image->height()) {
				source_map = cached_single_map_image;
				used_cached_scaled = true;
			}
			else {
				int64_t mapped_frame = frame_number;
				if (map_reader->info.has_video && map_reader->info.video_length > 0 &&
					map_reader->info.fps.num > 0 && map_reader->info.fps.den > 0) {
					double host_fps = 30.0;
					if (ParentTimeline()) {
						Timeline* parent_timeline = dynamic_cast<Timeline*>(ParentTimeline());
						if (parent_timeline && parent_timeline->info.fps.num > 0 && parent_timeline->info.fps.den > 0)
							host_fps = parent_timeline->info.fps.ToDouble();
					}
					double source_fps = map_reader->info.fps.ToDouble();
					if (host_fps > 0.0 && source_fps > 0.0) {
						const int64_t requested_index = std::max(int64_t(0), frame_number - 1);
						const double seconds = static_cast<double>(requested_index) / host_fps;
						mapped_frame = static_cast<int64_t>(std::floor(seconds * source_fps)) + 1;
					}
					mapped_frame = std::min(std::max(int64_t(1), mapped_frame), map_reader->info.video_length);
				}

				auto source_frame = map_reader->GetFrame(mapped_frame);
				if (source_frame && source_frame->GetImage() && !source_frame->GetImage()->isNull())
					source_map = std::make_shared<QImage>(*source_frame->GetImage());
			}
		} catch (const std::exception& e) {
			ZmqLogger::Instance()->Log(
				std::string("Displace::GetMapImage unable to read displacement frame: ") + e.what());
			source_map.reset();
		}
	}

	if (!source_map || source_map->isNull())
		return {};

	if (used_cached_scaled)
		return source_map;

	auto scaled_map = std::make_shared<QImage>(
		source_map->scaled(
			target_image->width(), target_image->height(),
			Qt::IgnoreAspectRatio, Qt::SmoothTransformation));
	if (map_reader->info.has_single_image) {
		cached_single_map_image = scaled_map;
		cached_single_map_width = target_image->width();
		cached_single_map_height = target_image->height();
	}
	return scaled_map;
}

// This method is required for all derived classes of EffectBase, and returns a
// modified openshot::Frame object
std::shared_ptr<openshot::Frame> Displace::GetFrame(std::shared_ptr<openshot::Frame> frame, int64_t frame_number) {
	std::shared_ptr<QImage> frame_image = frame->GetImage();
	if (!frame_image || frame_image->isNull())
		return frame;

	auto map_image = GetMapImage(frame_image, frame_number);
	if (!map_image || map_image->isNull())
		return frame;

	const int width = frame_image->width();
	const int height = frame_image->height();
	const int pixel_count = width * height;

	QImage original = frame_image->copy();
	const unsigned char* original_pixels = reinterpret_cast<const unsigned char*>(original.constBits());
	unsigned char* pixels = reinterpret_cast<unsigned char*>(frame_image->bits());
	const unsigned char* map_pixels = reinterpret_cast<const unsigned char*>(map_image->constBits());

	const float strength_value = static_cast<float>(strength.GetValue(frame_number));
	const float x_amount = static_cast<float>(horizontal.GetValue(frame_number)) * width * strength_value;
	const float y_amount = static_cast<float>(vertical.GetValue(frame_number)) * height * strength_value;
	const float contrast_value = static_cast<float>(contrast.GetValue(frame_number));
	const int brightness_adj = static_cast<int>(255.0f * brightness.GetValue(frame_number));
	const float contrast_factor = 20.0f / std::max(0.00001f, 20.0f - contrast_value);
	const bool output_map = replace_image;

	std::array<unsigned char, 256> adjusted_gray{};
	for (int gray = 0; gray < 256; ++gray) {
		const int adjusted = static_cast<int>(contrast_factor * ((gray + brightness_adj) - 128) + 128);
		adjusted_gray[gray] = clamp_u8(adjusted);
	}

	#pragma omp parallel for if(pixel_count >= 8192) schedule(static)
	for (int i = 0; i < pixel_count; ++i) {
		const int idx = i * 4;
		const int map_r = map_pixels[idx + 0];
		const int map_g = map_pixels[idx + 1];
		const int map_b = map_pixels[idx + 2];
		const int map_a = map_pixels[idx + 3];
		const int gray = ((map_r * 11) + (map_g * 16) + (map_b * 5)) >> 5;
		int processed = adjusted_gray[gray];
		if (invert)
			processed = 255 - processed;

		// Transparent areas in the displacement map smoothly collapse back to neutral.
		processed = 128 + (((processed - 128) * map_a) / 255);

		if (output_map) {
			const unsigned char debug_value = clamp_u8(processed);
			pixels[idx + 0] = debug_value;
			pixels[idx + 1] = debug_value;
			pixels[idx + 2] = debug_value;
			pixels[idx + 3] = 255;
			continue;
		}

		const float signed_map = (static_cast<float>(processed) - 127.5f) / 127.5f;
		const int pixel_x = i % width;
		const int pixel_y = i / width;
		const float sample_x = pixel_x + (signed_map * x_amount);
		const float sample_y = pixel_y + (signed_map * y_amount);

		pixels[idx + 0] = clamp_u8(static_cast<int>(std::lround(sample_bilinear(original_pixels, width, height, sample_x, sample_y, 0))));
		pixels[idx + 1] = clamp_u8(static_cast<int>(std::lround(sample_bilinear(original_pixels, width, height, sample_x, sample_y, 1))));
		pixels[idx + 2] = clamp_u8(static_cast<int>(std::lround(sample_bilinear(original_pixels, width, height, sample_x, sample_y, 2))));
		pixels[idx + 3] = clamp_u8(static_cast<int>(std::lround(sample_bilinear(original_pixels, width, height, sample_x, sample_y, 3))));
	}

	return frame;
}

// Generate JSON string of this object
std::string Displace::Json() const {
	return JsonValue().toStyledString();
}

// Generate Json::Value for this object
Json::Value Displace::JsonValue() const {
	Json::Value root = EffectBase::JsonValue();
	root["type"] = info.class_name;
	root["replace_image"] = replace_image;
	root["invert"] = invert;
	root["strength"] = strength.JsonValue();
	root["horizontal"] = horizontal.JsonValue();
	root["vertical"] = vertical.JsonValue();
	root["brightness"] = brightness.JsonValue();
	root["contrast"] = contrast.JsonValue();
	if (map_reader)
		root["map_reader"] = map_reader->JsonValue();
	else
		root["map_reader"] = Json::objectValue;
	return root;
}

// Load JSON string into this object
void Displace::SetJson(const std::string value) {
	try
	{
		const Json::Value root = openshot::stringToJson(value);
		SetJsonValue(root);
	}
	catch (const std::exception& e)
	{
		throw InvalidJSON("JSON is invalid (missing keys or invalid data types)");
	}
}

// Load Json::Value into this object
void Displace::SetJsonValue(const Json::Value root) {
	Json::Value normalized_root = root;
	// Keep displacement-map source separate from the shared effect mask reader.
	// EffectBase still treats a plain "reader" field as a legacy alias for
	// "mask_reader", so strip displacement-source fields before loading the
	// common effect-mask state.
	normalized_root.removeMember("reader");
	normalized_root.removeMember("map_reader");
	EffectBase::SetJsonValue(normalized_root);

	const Json::Value map_reader_json =
		!root["map_reader"].isNull() ? root["map_reader"] : root["reader"];
	if (!map_reader_json.isNull()) {
		if (!map_reader_json["type"].isNull())
			Reader(CreateReaderFromJson(map_reader_json));
		else if (map_reader_json.isObject() && map_reader_json.empty())
			Reader(NULL);
	}
	if (!root["replace_image"].isNull())
		replace_image = root["replace_image"].asBool();
	if (!root["invert"].isNull())
		invert = root["invert"].asBool();
	if (!root["strength"].isNull())
		strength.SetJsonValue(root["strength"]);
	if (!root["horizontal"].isNull())
		horizontal.SetJsonValue(root["horizontal"]);
	if (!root["vertical"].isNull())
		vertical.SetJsonValue(root["vertical"]);
	if (!root["brightness"].isNull())
		brightness.SetJsonValue(root["brightness"]);
	if (!root["contrast"].isNull())
		contrast.SetJsonValue(root["contrast"]);
}

// Get all properties for a specific frame
std::string Displace::PropertiesJSON(int64_t requested_frame) const {
	Json::Value root = BasePropertiesJSON(requested_frame);

	root["replace_image"] = add_property_json("Replace Image", replace_image, "int", "", NULL, 0, 1, false, requested_frame);
	root["replace_image"]["choices"].append(add_property_choice_json("Yes", true, replace_image));
	root["replace_image"]["choices"].append(add_property_choice_json("No", false, replace_image));
	root["invert"] = add_property_json("Map: Invert", invert, "int", "", NULL, 0, 1, false, requested_frame);
	root["invert"]["choices"].append(add_property_choice_json("Yes", true, invert));
	root["invert"]["choices"].append(add_property_choice_json("No", false, invert));
	if (map_reader)
		root["map_reader"] = add_property_json("Map: Source", 0.0, "reader", map_reader->Json(), NULL, 0, 1, false, requested_frame);
	else
		root["map_reader"] = add_property_json("Map: Source", 0.0, "reader", "{}", NULL, 0, 1, false, requested_frame);

	root["strength"] = add_property_json("Strength", strength.GetValue(requested_frame), "float", "", &strength, 0.0, 3.0, false, requested_frame);
	root["horizontal"] = add_property_json("Horizontal", horizontal.GetValue(requested_frame), "float", "", &horizontal, -1.0, 1.0, false, requested_frame);
	root["vertical"] = add_property_json("Vertical", vertical.GetValue(requested_frame), "float", "", &vertical, -1.0, 1.0, false, requested_frame);
	root["brightness"] = add_property_json("Brightness", brightness.GetValue(requested_frame), "float", "", &brightness, -1.0, 1.0, false, requested_frame);
	root["contrast"] = add_property_json("Contrast", contrast.GetValue(requested_frame), "float", "", &contrast, 0.0, 20.0, false, requested_frame);

	return root.toStyledString();
}

void Displace::Reader(ReaderBase *new_reader) {
	if (map_reader == new_reader)
		return;

	if (map_reader) {
		map_reader->Close();
		delete map_reader;
	}

	map_reader = new_reader;
	cached_single_map_image.reset();
	cached_single_map_width = 0;
	cached_single_map_height = 0;
	if (map_reader)
		map_reader->ParentClip(ParentClip());
}

Displace::~Displace() {
	Reader(NULL);
}
