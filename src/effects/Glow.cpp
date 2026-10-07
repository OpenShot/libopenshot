/**
 * @file
 * @brief Source file for Glow effect class
 * @author Jonathan Thomas <jonathan@openshot.org>
 *
 * @ref License
 */

// Copyright (c) 2008-2026 OpenShot Studios, LLC
//
// SPDX-License-Identifier: LGPL-3.0-or-later

#include "Glow.h"

#include "Exceptions.h"

#include <QPainter>
#include <cmath>
#include <vector>
#include <omp.h>

using namespace openshot;

namespace {
	inline float glow_clampf(float value, float low, float high) {
		if (value < low)
			return low;
		if (value > high)
			return high;
		return value;
	}

	inline void glow_blur_horizontal(const std::vector<float>& src, std::vector<float>& dst, int w, int h, int radius) {
		if (radius <= 0) {
			dst = src;
			return;
		}

		const float inv = 1.0f / static_cast<float>((radius * 2) + 1);
		#pragma omp parallel for schedule(static)
		for (int y = 0; y < h; ++y) {
			int ti = y * w;
			int li = ti;
			int ri = ti + radius;
			float first = src[ti];
			float last = src[ti + w - 1];
			float value = static_cast<float>(radius + 1) * first;
			for (int j = 0; j < radius; ++j)
				value += src[ti + j];
			for (int x = 0; x <= radius; ++x) {
				value += src[ri++] - first;
				dst[ti++] = value * inv;
			}
			for (int x = radius + 1; x < w - radius; ++x) {
				value += src[ri++] - src[li++];
				dst[ti++] = value * inv;
			}
			for (int x = w - radius; x < w; ++x) {
				value += last - src[li++];
				dst[ti++] = value * inv;
			}
		}
	}

	inline void glow_blur_vertical(const std::vector<float>& src, std::vector<float>& dst, int w, int h, int radius) {
		if (radius <= 0) {
			dst = src;
			return;
		}

		const float inv = 1.0f / static_cast<float>((radius * 2) + 1);
		#pragma omp parallel for schedule(static)
		for (int x = 0; x < w; ++x) {
			int ti = x;
			int li = ti;
			int ri = ti + (radius * w);
			float first = src[ti];
			float last = src[ti + (w * (h - 1))];
			float value = static_cast<float>(radius + 1) * first;
			for (int j = 0; j < radius; ++j)
				value += src[ti + (j * w)];
			for (int y = 0; y <= radius; ++y) {
				value += src[ri] - first;
				dst[ti] = value * inv;
				ri += w;
				ti += w;
			}
			for (int y = radius + 1; y < h - radius; ++y) {
				value += src[ri] - src[li];
				dst[ti] = value * inv;
				li += w;
				ri += w;
				ti += w;
			}
			for (int y = h - radius; y < h; ++y) {
				value += last - src[li];
				dst[ti] = value * inv;
				li += w;
				ti += w;
			}
		}
	}

	inline void glow_apply_box_blur(std::vector<float>& mask, int w, int h, int radius, int iterations) {
		if (radius <= 0 || iterations <= 0 || w <= 0 || h <= 0)
			return;

		std::vector<float> temp(mask.size());
		for (int i = 0; i < iterations; ++i) {
			glow_blur_horizontal(mask, temp, w, h, radius);
			glow_blur_vertical(temp, mask, w, h, radius);
		}
	}

	inline void glow_spread_horizontal(const std::vector<float>& src, std::vector<float>& dst, int w, int h, int radius) {
		if (radius <= 0) {
			dst = src;
			return;
		}

		#pragma omp parallel for schedule(static)
		for (int y = 0; y < h; ++y) {
			const int row = y * w;
			for (int x = 0; x < w; ++x) {
				const int left = std::max(0, x - radius);
				const int right = std::min(w - 1, x + radius);
				float value = 0.0f;
				for (int sample_x = left; sample_x <= right; ++sample_x)
					value = std::max(value, src[row + sample_x]);
				dst[row + x] = value;
			}
		}
	}

	inline void glow_spread_vertical(const std::vector<float>& src, std::vector<float>& dst, int w, int h, int radius) {
		if (radius <= 0) {
			dst = src;
			return;
		}

		#pragma omp parallel for schedule(static)
		for (int y = 0; y < h; ++y) {
			const int top = std::max(0, y - radius);
			const int bottom = std::min(h - 1, y + radius);
			for (int x = 0; x < w; ++x) {
				float value = 0.0f;
				for (int sample_y = top; sample_y <= bottom; ++sample_y)
					value = std::max(value, src[(sample_y * w) + x]);
				dst[(y * w) + x] = value;
			}
		}
	}

	inline void glow_apply_spread(std::vector<float>& mask, int w, int h, int radius) {
		if (radius <= 0 || w <= 0 || h <= 0)
			return;

		std::vector<float> temp(mask.size());
		glow_spread_horizontal(mask, temp, w, h, radius);
		glow_spread_vertical(temp, mask, w, h, radius);
	}
}

Glow::Glow()
	: mode(GLOW_MODE_OUTER), opacity(0.45), blur_radius(20.0), spread(0.10), color("#fff4c2") {
	init_effect_details();
}

Glow::Glow(Keyframe new_opacity, Keyframe new_blur_radius, Keyframe new_spread, Color new_color)
	: mode(GLOW_MODE_OUTER), opacity(new_opacity), blur_radius(new_blur_radius), spread(new_spread), color(new_color) {
	init_effect_details();
}

void Glow::init_effect_details()
{
	InitEffectInfo();
	info.class_name = "Glow";
	info.name = "Glow";
	info.description = "Add an outer or inner glow based on visible pixels.";
	info.has_audio = false;
	info.has_video = true;
	info.apply_before_clip = false;
}

std::shared_ptr<openshot::Frame> Glow::GetFrame(std::shared_ptr<openshot::Frame> frame, int64_t frame_number)
{
	auto frame_image = frame->GetImage();
	if (!frame_image || frame_image->isNull())
		return frame;

	const int w = frame_image->width();
	const int h = frame_image->height();
	if (w <= 0 || h <= 0)
		return frame;

	QImage source = frame_image->copy();
	const unsigned char* source_pixels = reinterpret_cast<const unsigned char*>(source.constBits());

	const float opacity_value = glow_clampf(static_cast<float>(opacity.GetValue(frame_number)), 0.0f, 1.0f);
	const int blur_value = std::max(0, blur_radius.GetInt(frame_number));
	const float spread_value = glow_clampf(static_cast<float>(spread.GetValue(frame_number)), 0.0f, 1.0f);
	const std::vector<int> rgba = color.GetColorRGBA(frame_number);
	const float color_r = static_cast<float>(rgba[0]);
	const float color_g = static_cast<float>(rgba[1]);
	const float color_b = static_cast<float>(rgba[2]);
	const float color_a = static_cast<float>(rgba[3]) / 255.0f;

	std::vector<float> alpha_mask(static_cast<size_t>(w * h));
	#pragma omp parallel for schedule(static)
	for (int i = 0; i < (w * h); ++i) {
		const float alpha = static_cast<float>(source_pixels[(i * 4) + 3]) / 255.0f;
		alpha_mask[i] = alpha;
	}

	const int spread_radius = std::max(0, static_cast<int>(std::lround(static_cast<float>(blur_value) * spread_value)));
	if (spread_radius > 0)
		glow_apply_spread(alpha_mask, w, h, spread_radius);

	std::vector<float> blurred_mask = alpha_mask;
	if (blur_value > 0)
		glow_apply_box_blur(blurred_mask, w, h, blur_value, 3);

	QImage glow_overlay(w, h, QImage::Format_RGBA8888_Premultiplied);
	glow_overlay.fill(Qt::transparent);
	unsigned char* glow_pixels = reinterpret_cast<unsigned char*>(glow_overlay.bits());

	if (mode == GLOW_MODE_OUTER) {
		#pragma omp parallel for schedule(static)
		for (int i = 0; i < (w * h); ++i) {
			const float overlay_alpha = glow_clampf(blurred_mask[i] * opacity_value * color_a, 0.0f, 1.0f);
			const int idx = i * 4;
			glow_pixels[idx + 0] = static_cast<unsigned char>(color_r * overlay_alpha);
			glow_pixels[idx + 1] = static_cast<unsigned char>(color_g * overlay_alpha);
			glow_pixels[idx + 2] = static_cast<unsigned char>(color_b * overlay_alpha);
			glow_pixels[idx + 3] = static_cast<unsigned char>(255.0f * overlay_alpha);
		}
	}
	else {
		std::vector<float> inverse_mask(static_cast<size_t>(w * h));
		#pragma omp parallel for schedule(static)
		for (int i = 0; i < (w * h); ++i)
			inverse_mask[i] = 1.0f - alpha_mask[i];

		if (blur_value > 0)
			glow_apply_box_blur(inverse_mask, w, h, blur_value, 3);

		#pragma omp parallel for schedule(static)
		for (int i = 0; i < (w * h); ++i) {
			const float inner = glow_clampf(inverse_mask[i] * alpha_mask[i] * opacity_value * color_a, 0.0f, 1.0f);
			const int idx = i * 4;
			glow_pixels[idx + 0] = static_cast<unsigned char>(color_r * inner);
			glow_pixels[idx + 1] = static_cast<unsigned char>(color_g * inner);
			glow_pixels[idx + 2] = static_cast<unsigned char>(color_b * inner);
			glow_pixels[idx + 3] = static_cast<unsigned char>(255.0f * inner);
		}
	}

	QImage result(w, h, QImage::Format_RGBA8888_Premultiplied);
	result.fill(Qt::transparent);
	{
		QPainter painter(&result);
		painter.setRenderHints(QPainter::Antialiasing | QPainter::SmoothPixmapTransform, true);
		if (mode == GLOW_MODE_OUTER) {
			painter.drawImage(0, 0, glow_overlay);
			painter.drawImage(0, 0, source);
		} else {
			painter.drawImage(0, 0, source);
			painter.drawImage(0, 0, glow_overlay);
		}
	}

	*frame_image = result;
	return frame;
}

std::string Glow::Json() const {
	return JsonValue().toStyledString();
}

Json::Value Glow::JsonValue() const {
	Json::Value root = EffectBase::JsonValue();
	root["type"] = info.class_name;
	root["mode"] = mode;
	root["opacity"] = opacity.JsonValue();
	root["blur_radius"] = blur_radius.JsonValue();
	root["spread"] = spread.JsonValue();
	root["color"] = color.JsonValue();
	return root;
}

void Glow::SetJson(const std::string value) {
	try {
		const Json::Value root = openshot::stringToJson(value);
		SetJsonValue(root);
	}
	catch (const std::exception& e) {
		throw InvalidJSON("JSON is invalid (missing keys or invalid data types)");
	}
}

void Glow::SetJsonValue(const Json::Value root) {
	EffectBase::SetJsonValue(root);
	if (!root["mode"].isNull())
		mode = root["mode"].asInt();
	if (!root["opacity"].isNull())
		opacity.SetJsonValue(root["opacity"]);
	if (!root["blur_radius"].isNull())
		blur_radius.SetJsonValue(root["blur_radius"]);
	if (!root["spread"].isNull())
		spread.SetJsonValue(root["spread"]);
	if (!root["color"].isNull())
		color.SetJsonValue(root["color"]);
}

std::string Glow::PropertiesJSON(int64_t requested_frame) const {
	Json::Value root = BasePropertiesJSON(requested_frame);
	root["mode"] = add_property_json("Mode", mode, "int", "", NULL, 0, 1, false, requested_frame);
	root["mode"]["choices"].append(add_property_choice_json("Outer", GLOW_MODE_OUTER, mode));
	root["mode"]["choices"].append(add_property_choice_json("Inner", GLOW_MODE_INNER, mode));
	root["opacity"] = add_property_json("Opacity", opacity.GetValue(requested_frame), "float", "", &opacity, 0.0, 1.0, false, requested_frame);
	root["blur_radius"] = add_property_json("Blur Radius", blur_radius.GetValue(requested_frame), "float", "", &blur_radius, 0.0, 100.0, false, requested_frame);
	root["spread"] = add_property_json("Spread", spread.GetValue(requested_frame), "float", "", &spread, 0.0, 1.0, false, requested_frame);
	root["color"] = add_property_json("Color", 0.0, "color", "", &color.red, 0, 255, false, requested_frame);
	root["color"]["red"] = add_property_json("Red", color.red.GetValue(requested_frame), "float", "", &color.red, 0, 255, false, requested_frame);
	root["color"]["green"] = add_property_json("Green", color.green.GetValue(requested_frame), "float", "", &color.green, 0, 255, false, requested_frame);
	root["color"]["blue"] = add_property_json("Blue", color.blue.GetValue(requested_frame), "float", "", &color.blue, 0, 255, false, requested_frame);
	root["color"]["alpha"] = add_property_json("Alpha", color.alpha.GetValue(requested_frame), "float", "", &color.alpha, 0, 255, false, requested_frame);
	return root.toStyledString();
}
