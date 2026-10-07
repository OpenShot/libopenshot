/**
 * @file
 * @brief Source file for ColorGrade effect
 * @author Jonathan Thomas <jonathan@openshot.org>
 *
 * @ref License
 */

// Copyright (c) 2008-2026 OpenShot Studios, LLC
//
// SPDX-License-Identifier: LGPL-3.0-or-later

#include "ColorGrade.h"
#include "Exceptions.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <sstream>

using namespace openshot;

namespace {
constexpr float kInv255 = 1.0f / 255.0f;

struct WheelBiasParams {
	float red_delta;
	float green_delta;
	float blue_delta;
};

static inline float clamp01(float value) {
	return std::max(0.0f, std::min(1.0f, value));
}

static inline int clampByte(float value) {
	if (value <= 0.0f)
		return 0;
	if (value >= 255.0f)
		return 255;
	return static_cast<int>(value + 0.5f);
}

static inline float smooth_midtones(float luma) {
	const float centered = std::abs(luma - 0.5f) * 2.0f;
	return clamp01(1.0f - centered * centered);
}

static std::string color_to_hex(const QColor& color) {
	return color.name(QColor::HexRgb).toStdString();
}

static std::array<float, 256> build_curve_lut(const AnimatedCurve& curve, int64_t frame_number) {
	std::array<float, 256> lut{};
	if (curve.enabled.GetValue(frame_number) < 0.5) {
		for (size_t i = 0; i < lut.size(); ++i)
			lut[i] = static_cast<float>(i) * kInv255;
		return lut;
	}

	const Keyframe sampled_curve = curve.BuildCurve(frame_number, 255.0);
	for (size_t i = 0; i < lut.size(); ++i) {
		lut[i] = clamp01(static_cast<float>(sampled_curve.GetValue(static_cast<int64_t>(i))));
	}
	return lut;
}

static float sample_curve_lut(const std::array<float, 256>& lut, float value) {
	return lut[static_cast<int>(value * 255.0f + 0.5f)];
}

static WheelBiasParams build_wheel_bias(const ColorGradeWheelEntry& wheel, int64_t frame_number) {
	const QColor color = wheel.GetColor(frame_number);
	const float cr = color.redF();
	const float cg = color.greenF();
	const float cb = color.blueF();
	const float avg = (cr + cg + cb) / 3.0f;
	const float amount = wheel.GetAmount(frame_number);
	const float luma = wheel.GetLuma(frame_number);
	return {
		((cr - avg) * amount) + luma,
		((cg - avg) * amount) + luma,
		((cb - avg) * amount) + luma,
	};
}
}

ColorGradeWheelEntry::ColorGradeWheelEntry()
	: color(QColor(Qt::white)), amount(0.0f), luma(0.0f) {}

Json::Value ColorGradeWheelEntry::JsonValue() const {
	Json::Value root(Json::objectValue);
	root["color"] = color_to_hex(QColor(
		color.red.GetInt(1),
		color.green.GetInt(1),
		color.blue.GetInt(1),
		color.alpha.GetInt(1)));
	root["color_keyframes"] = color.JsonValue();
	root["amount"] = amount.GetValue(1);
	root["amount_keyframes"] = amount.JsonValue();
	root["luma"] = luma.GetValue(1);
	root["luma_keyframes"] = luma.JsonValue();
	return root;
}

void ColorGradeWheelEntry::SetJsonValue(const Json::Value& root) {
	if (!root["color_keyframes"].isNull()) {
		color.SetJsonValue(root["color_keyframes"]);
	} else if (!root["color"].isNull()) {
		const QColor parsed(QString::fromStdString(root["color"].asString()));
		if (parsed.isValid())
			color = Color(parsed);
	}
	if (!root["amount_keyframes"].isNull())
		amount.SetJsonValue(root["amount_keyframes"]);
	else if (!root["amount"].isNull())
		amount = Keyframe(std::max(-1.0f, std::min(1.0f, root["amount"].asFloat())));
	if (!root["luma_keyframes"].isNull())
		luma.SetJsonValue(root["luma_keyframes"]);
	else if (!root["luma"].isNull())
		luma = Keyframe(std::max(-1.0f, std::min(1.0f, root["luma"].asFloat())));
}

QColor ColorGradeWheelEntry::GetColor(int64_t frame_number) const {
	return QColor(
		color.red.GetInt(frame_number),
		color.green.GetInt(frame_number),
		color.blue.GetInt(frame_number),
		color.alpha.GetInt(frame_number));
}

float ColorGradeWheelEntry::GetAmount(int64_t frame_number) const {
	return std::max(-1.0f, std::min(1.0f, static_cast<float>(amount.GetValue(frame_number))));
}

float ColorGradeWheelEntry::GetLuma(int64_t frame_number) const {
	return std::max(-1.0f, std::min(1.0f, static_cast<float>(luma.GetValue(frame_number))));
}

ColorGradeWheelsData::ColorGradeWheelsData()
	: enabled(1.0) {}

Json::Value ColorGradeWheelsData::JsonValue() const {
	Json::Value root(Json::objectValue);
	root["enabled"] = enabled.GetValue(1) >= 0.5;
	root["enabled_keyframes"] = enabled.JsonValue();
	root["global"] = global.JsonValue();
	root["shadows"] = shadows.JsonValue();
	root["midtones"] = midtones.JsonValue();
	root["highlights"] = highlights.JsonValue();
	return root;
}

void ColorGradeWheelsData::SetJsonValue(const Json::Value& root) {
	if (root["enabled"].isBool())
		enabled = Keyframe(root["enabled"].asBool() ? 1.0 : 0.0);
	else if (!root["enabled_keyframes"].isNull())
		enabled.SetJsonValue(root["enabled_keyframes"]);
	if (!root["global"].isNull())
		global.SetJsonValue(root["global"]);
	if (!root["shadows"].isNull())
		shadows.SetJsonValue(root["shadows"]);
	if (!root["midtones"].isNull())
		midtones.SetJsonValue(root["midtones"]);
	if (!root["highlights"].isNull())
		highlights.SetJsonValue(root["highlights"]);
}

std::string ColorGradeWheelsData::Summary(int64_t frame_number) const {
	return IsEnabled(frame_number) ? "Global / Shadows / Midtones / Highlights"
	                               : "Disabled";
}

bool ColorGradeWheelsData::IsEnabled(int64_t frame_number) const {
	return enabled.GetValue(frame_number) >= 0.5;
}

ColorGrade::ColorGrade()
	: temperature(0.0),
	  tint(0.0),
	  exposure(0.0),
	  contrast(0.0),
	  highlights(0.0),
	  shadows(0.0),
	  saturation(1.0),
	  vibrance(0.0),
	  mix(1.0),
	  lut_intensity(1.0),
	  lut_path(""),
	  lut_dirty(true)
{
	init_effect_details();
}

void ColorGrade::init_effect_details() {
	InitEffectInfo();
	info.class_name = "ColorGrade";
	info.name = "Color Grade";
	info.description = "Unified color grading effect with curves, wheels and LUT support.";
	info.has_audio = false;
	info.has_video = true;
}

float ColorGrade::Clamp01(float value) {
	return clamp01(value);
}

void ColorGrade::sync_lut_effect() {
	if (!lut_dirty)
		return;

	Json::Value payload(Json::objectValue);
	payload["lut_path"] = lut_path;
	payload["intensity"] = lut_intensity.JsonValue();
	payload["intensity_r"] = Keyframe(1.0).JsonValue();
	payload["intensity_g"] = Keyframe(1.0).JsonValue();
	payload["intensity_b"] = Keyframe(1.0).JsonValue();
	lut_effect.SetJsonValue(payload);
	lut_dirty = false;
}

std::shared_ptr<openshot::Frame> ColorGrade::GetFrame(std::shared_ptr<openshot::Frame> frame, int64_t frame_number) {
	std::shared_ptr<QImage> frame_image = frame->GetImage();
	if (!frame_image)
		return frame;

	const float temperature_value = std::max(-1.0f, std::min(1.0f, static_cast<float>(temperature.GetValue(frame_number))));
	const float tint_value = std::max(-1.0f, std::min(1.0f, static_cast<float>(tint.GetValue(frame_number))));
	const float exposure_value = std::max(-4.0f, std::min(4.0f, static_cast<float>(exposure.GetValue(frame_number))));
	const float contrast_value = std::max(-1.0f, std::min(2.0f, static_cast<float>(contrast.GetValue(frame_number))));
	const float highlights_value = std::max(-1.0f, std::min(1.0f, static_cast<float>(highlights.GetValue(frame_number))));
	const float shadows_value = std::max(-1.0f, std::min(1.0f, static_cast<float>(shadows.GetValue(frame_number))));
	const float saturation_value = std::max(0.0f, std::min(4.0f, static_cast<float>(saturation.GetValue(frame_number))));
	const float vibrance_value = std::max(-1.0f, std::min(1.0f, static_cast<float>(vibrance.GetValue(frame_number))));
	const float mix_value = std::max(0.0f, std::min(1.0f, static_cast<float>(mix.GetValue(frame_number))));
	const float inverse_mix = 1.0f - mix_value;
	const bool wheels_enabled = wheels.IsEnabled(frame_number);
	if (temperature_value == 0.0f && tint_value == 0.0f &&
		exposure_value == 0.0f && contrast_value == 0.0f &&
		highlights_value == 0.0f && shadows_value == 0.0f &&
		saturation_value == 1.0f && vibrance_value == 0.0f &&
		mix_value == 1.0f && !wheels_enabled &&
		curve_all.enabled.GetValue(frame_number) < 0.5 &&
		curve_red.enabled.GetValue(frame_number) < 0.5 &&
		curve_green.enabled.GetValue(frame_number) < 0.5 &&
		curve_blue.enabled.GetValue(frame_number) < 0.5) {
		if (!lut_path.empty() && lut_intensity.GetValue(frame_number) > 0.0) {
			sync_lut_effect();
			return lut_effect.GetFrame(frame, frame_number);
		}
		return frame;
	}
	const float exposure_gain = std::pow(2.0f, exposure_value);
	const float contrast_factor = std::max(0.0f, 1.0f + contrast_value);
	const std::array<float, 256> curve_all_lut = build_curve_lut(curve_all, frame_number);
	const std::array<float, 256> curve_red_lut = build_curve_lut(curve_red, frame_number);
	const std::array<float, 256> curve_green_lut = build_curve_lut(curve_green, frame_number);
	const std::array<float, 256> curve_blue_lut = build_curve_lut(curve_blue, frame_number);
	const WheelBiasParams global_wheel = wheels_enabled ? build_wheel_bias(wheels.global, frame_number) : WheelBiasParams{0.0f, 0.0f, 0.0f};
	const WheelBiasParams shadows_wheel = wheels_enabled ? build_wheel_bias(wheels.shadows, frame_number) : WheelBiasParams{0.0f, 0.0f, 0.0f};
	const WheelBiasParams midtones_wheel = wheels_enabled ? build_wheel_bias(wheels.midtones, frame_number) : WheelBiasParams{0.0f, 0.0f, 0.0f};
	const WheelBiasParams highlights_wheel = wheels_enabled ? build_wheel_bias(wheels.highlights, frame_number) : WheelBiasParams{0.0f, 0.0f, 0.0f};

	static const std::array<float, 256> inv_alpha = [] {
		std::array<float, 256> lut{};
		lut[0] = 0.0f;
		for (int i = 1; i < 256; ++i)
			lut[i] = 255.0f / static_cast<float>(i);
		return lut;
	}();

	const auto apply_wheel_bias = [](const WheelBiasParams& wheel, float weight, float& r, float& g, float& b) {
		if (std::abs(weight) <= 0.00001f)
			return;
		r += wheel.red_delta * weight;
		g += wheel.green_delta * weight;
		b += wheel.blue_delta * weight;
	};

	unsigned char* pixels = reinterpret_cast<unsigned char*>(frame_image->bits());
	const int pixel_count = frame_image->width() * frame_image->height();

	#pragma omp parallel for if(pixel_count >= 16384) schedule(static)
	for (int pixel = 0; pixel < pixel_count; ++pixel) {
		const int idx = pixel * 4;
		const int A = pixels[idx + 3];
		if (A <= 0)
			continue;

		const float alpha_percent = static_cast<float>(A) * kInv255;
		float R = 0.0f;
		float G = 0.0f;
		float B = 0.0f;

		if (A == 255) {
			R = pixels[idx + 0] * kInv255;
			G = pixels[idx + 1] * kInv255;
			B = pixels[idx + 2] * kInv255;
		} else {
			const float inv_alpha_percent = inv_alpha[A];
			R = (pixels[idx + 0] * inv_alpha_percent) * kInv255;
			G = (pixels[idx + 1] * inv_alpha_percent) * kInv255;
			B = (pixels[idx + 2] * inv_alpha_percent) * kInv255;
		}

		const float original_r = R;
		const float original_g = G;
		const float original_b = B;

		// White-balance style controls.
		R = Clamp01(R + (temperature_value * 0.125f));
		B = Clamp01(B - (temperature_value * 0.125f));
		G = Clamp01(G - (tint_value * 0.1f));
		R = Clamp01(R + (tint_value * 0.05f));
		B = Clamp01(B + (tint_value * 0.05f));

		// Exposure and contrast.
		R = Clamp01(R * exposure_gain);
		G = Clamp01(G * exposure_gain);
		B = Clamp01(B * exposure_gain);

		R = Clamp01(((R - 0.5f) * contrast_factor) + 0.5f);
		G = Clamp01(((G - 0.5f) * contrast_factor) + 0.5f);
		B = Clamp01(((B - 0.5f) * contrast_factor) + 0.5f);

		float luma = (0.299f * R) + (0.587f * G) + (0.114f * B);
		const float shadow_weight = (1.0f - luma) * (1.0f - luma);
		const float highlight_weight = luma * luma;

		// Shadows/highlights recovery.
		const float shadow_adjust = shadows_value * shadow_weight * 0.35f;
		const float highlight_adjust = highlights_value * highlight_weight * 0.35f;
		R = Clamp01(R + shadow_adjust + highlight_adjust);
		G = Clamp01(G + shadow_adjust + highlight_adjust);
		B = Clamp01(B + shadow_adjust + highlight_adjust);

		// Tonal wheels.
		luma = (0.299f * R) + (0.587f * G) + (0.114f * B);
		float wheel_r = R;
		float wheel_g = G;
		float wheel_b = B;
		if (wheels_enabled) {
			apply_wheel_bias(global_wheel, 1.0f, wheel_r, wheel_g, wheel_b);
			apply_wheel_bias(shadows_wheel, (1.0f - luma) * (1.0f - luma), wheel_r, wheel_g, wheel_b);
			apply_wheel_bias(midtones_wheel, smooth_midtones(luma), wheel_r, wheel_g, wheel_b);
			apply_wheel_bias(highlights_wheel, luma * luma, wheel_r, wheel_g, wheel_b);
		}
		R = Clamp01(wheel_r);
		G = Clamp01(wheel_g);
		B = Clamp01(wheel_b);

		// Saturation and vibrance.
		luma = (0.299f * R) + (0.587f * G) + (0.114f * B);
		const float max_channel = std::max(R, std::max(G, B));
		const float min_channel = std::min(R, std::min(G, B));
		const float colorfulness = max_channel - min_channel;
		const float vibrance_factor = 1.0f + (vibrance_value * (1.0f - colorfulness));
		const float sat_factor = saturation_value * std::max(0.0f, vibrance_factor);
		R = Clamp01(luma + ((R - luma) * sat_factor));
		G = Clamp01(luma + ((G - luma) * sat_factor));
		B = Clamp01(luma + ((B - luma) * sat_factor));

		// Curves.
		R = sample_curve_lut(curve_all_lut, R);
		G = sample_curve_lut(curve_all_lut, G);
		B = sample_curve_lut(curve_all_lut, B);
		R = sample_curve_lut(curve_red_lut, R);
		G = sample_curve_lut(curve_green_lut, G);
		B = sample_curve_lut(curve_blue_lut, B);

		// Mix original and graded values.
		R = Clamp01((original_r * inverse_mix) + (R * mix_value));
		G = Clamp01((original_g * inverse_mix) + (G * mix_value));
		B = Clamp01((original_b * inverse_mix) + (B * mix_value));

		if (A == 255) {
			pixels[idx + 0] = static_cast<unsigned char>(clampByte(R * 255.0f));
			pixels[idx + 1] = static_cast<unsigned char>(clampByte(G * 255.0f));
			pixels[idx + 2] = static_cast<unsigned char>(clampByte(B * 255.0f));
		} else {
			pixels[idx + 0] = static_cast<unsigned char>(clampByte(R * 255.0f * alpha_percent));
			pixels[idx + 1] = static_cast<unsigned char>(clampByte(G * 255.0f * alpha_percent));
			pixels[idx + 2] = static_cast<unsigned char>(clampByte(B * 255.0f * alpha_percent));
		}
	}

	if (!lut_path.empty() && lut_intensity.GetValue(frame_number) > 0.0) {
		sync_lut_effect();
		frame = lut_effect.GetFrame(frame, frame_number);
	}

	return frame;
}

std::string ColorGrade::Json() const {
	return JsonValue().toStyledString();
}

Json::Value ColorGrade::JsonValue() const {
	Json::Value root = EffectBase::JsonValue();
	root["type"] = info.class_name;
	root["temperature"] = temperature.JsonValue();
	root["tint"] = tint.JsonValue();
	root["exposure"] = exposure.JsonValue();
	root["contrast"] = contrast.JsonValue();
	root["highlights"] = highlights.JsonValue();
	root["shadows"] = shadows.JsonValue();
	root["saturation"] = saturation.JsonValue();
	root["vibrance"] = vibrance.JsonValue();
	root["mix"] = mix.JsonValue();
	root["wheels"] = wheels.JsonValue();
	root["curve_all"] = curve_all.JsonValue();
	root["curve_red"] = curve_red.JsonValue();
	root["curve_green"] = curve_green.JsonValue();
	root["curve_blue"] = curve_blue.JsonValue();
	root["lut_path"] = lut_path;
	root["lut_intensity"] = lut_intensity.JsonValue();
	return root;
}

void ColorGrade::SetJson(const std::string value) {
	try {
		const Json::Value root = openshot::stringToJson(value);
		SetJsonValue(root);
	} catch (...) {
		throw InvalidJSON("Invalid JSON for ColorGrade effect");
	}
}

void ColorGrade::SetJsonValue(const Json::Value root) {
	EffectBase::SetJsonValue(root);

	if (!root["temperature"].isNull())
		temperature.SetJsonValue(root["temperature"]);
	if (!root["tint"].isNull())
		tint.SetJsonValue(root["tint"]);
	if (!root["exposure"].isNull())
		exposure.SetJsonValue(root["exposure"]);
	if (!root["contrast"].isNull())
		contrast.SetJsonValue(root["contrast"]);
	if (!root["highlights"].isNull())
		highlights.SetJsonValue(root["highlights"]);
	if (!root["shadows"].isNull())
		shadows.SetJsonValue(root["shadows"]);
	if (!root["saturation"].isNull())
		saturation.SetJsonValue(root["saturation"]);
	if (!root["vibrance"].isNull())
		vibrance.SetJsonValue(root["vibrance"]);
	if (!root["mix"].isNull())
		mix.SetJsonValue(root["mix"]);
	if (!root["wheels"].isNull())
		wheels.SetJsonValue(root["wheels"]);
	if (!root["curve_all"].isNull())
		curve_all.SetJsonValue(root["curve_all"]);
	if (!root["curve_red"].isNull())
		curve_red.SetJsonValue(root["curve_red"]);
	if (!root["curve_green"].isNull())
		curve_green.SetJsonValue(root["curve_green"]);
	if (!root["curve_blue"].isNull())
		curve_blue.SetJsonValue(root["curve_blue"]);
	if (!root["lut_path"].isNull()) {
		lut_path = root["lut_path"].asString();
		lut_dirty = true;
	}
	if (!root["lut_intensity"].isNull()) {
		lut_intensity.SetJsonValue(root["lut_intensity"]);
		lut_dirty = true;
	}
}

std::string ColorGrade::PropertiesJSON(int64_t requested_frame) const {
	Json::Value root = BasePropertiesJSON(requested_frame);

	root["temperature"] = add_property_json("Temperature", temperature.GetValue(requested_frame), "float", "", &temperature, -1.0, 1.0, false, requested_frame);
	root["tint"] = add_property_json("Tint", tint.GetValue(requested_frame), "float", "", &tint, -1.0, 1.0, false, requested_frame);
	root["exposure"] = add_property_json("Exposure", exposure.GetValue(requested_frame), "float", "", &exposure, -2.0, 2.0, false, requested_frame);
	root["contrast"] = add_property_json("Contrast", contrast.GetValue(requested_frame), "float", "", &contrast, -1.0, 1.0, false, requested_frame);
	root["highlights"] = add_property_json("Highlights", highlights.GetValue(requested_frame), "float", "", &highlights, -1.0, 1.0, false, requested_frame);
	root["shadows"] = add_property_json("Shadows", shadows.GetValue(requested_frame), "float", "", &shadows, -1.0, 1.0, false, requested_frame);
	root["saturation"] = add_property_json("Saturation", saturation.GetValue(requested_frame), "float", "", &saturation, 0.0, 4.0, false, requested_frame);
	root["vibrance"] = add_property_json("Vibrance", vibrance.GetValue(requested_frame), "float", "", &vibrance, -1.0, 1.0, false, requested_frame);
	root["mix"] = add_property_json("Mix", mix.GetValue(requested_frame), "float", "", &mix, 0.0, 1.0, false, requested_frame);

	root["wheels"] = add_property_json("Color Wheels", 0.0, "colorgrade_wheels", wheels.Summary(requested_frame), NULL, 0.0, 1.0, false, requested_frame);
	root["wheels"]["wheels"] = wheels.JsonValue();
	root["wheels"]["summary"] = wheels.Summary(requested_frame);

	root["curve_all"] = add_property_json("Curve: All", 0.0, "colorgrade_curve", curve_all.Summary(requested_frame), NULL, 0.0, 1.0, false, requested_frame);
	root["curve_all"]["curve"] = curve_all.JsonValue();
	root["curve_all"]["channel"] = "all";
	root["curve_all"]["summary"] = curve_all.Summary(requested_frame);

	root["curve_red"] = add_property_json("Curve: Red", 0.0, "colorgrade_curve", curve_red.Summary(requested_frame), NULL, 0.0, 1.0, false, requested_frame);
	root["curve_red"]["curve"] = curve_red.JsonValue();
	root["curve_red"]["channel"] = "red";
	root["curve_red"]["summary"] = curve_red.Summary(requested_frame);

	root["curve_green"] = add_property_json("Curve: Green", 0.0, "colorgrade_curve", curve_green.Summary(requested_frame), NULL, 0.0, 1.0, false, requested_frame);
	root["curve_green"]["curve"] = curve_green.JsonValue();
	root["curve_green"]["channel"] = "green";
	root["curve_green"]["summary"] = curve_green.Summary(requested_frame);

	root["curve_blue"] = add_property_json("Curve: Blue", 0.0, "colorgrade_curve", curve_blue.Summary(requested_frame), NULL, 0.0, 1.0, false, requested_frame);
	root["curve_blue"]["curve"] = curve_blue.JsonValue();
	root["curve_blue"]["channel"] = "blue";
	root["curve_blue"]["summary"] = curve_blue.Summary(requested_frame);

	root["lut_path"] = add_property_json("LUT File", 0.0, "string", lut_path, NULL, 0, 0, false, requested_frame);
	root["lut_intensity"] = add_property_json("LUT Intensity", lut_intensity.GetValue(requested_frame), "float", "", &lut_intensity, 0.0, 1.0, false, requested_frame);

	return root.toStyledString();
}
