/**
 * @file
 * @brief Source file for FilmGrain effect
 * @author Jonathan Thomas <jonathan@openshot.org>
 *
 * @ref License
 */

// Copyright (c) 2008-2026 OpenShot Studios, LLC
//
// SPDX-License-Identifier: LGPL-3.0-or-later

#include "FilmGrain.h"
#include "Clip.h"
#include "Exceptions.h"
#include "Timeline.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>

using namespace openshot;

namespace {
constexpr float kInv255    = 1.0f / 255.0f;
constexpr float kInvU32Max = 2.0f / 4294967295.0f;  // multiply instead of divide in hash_signed

// Salt constants for each grain channel and noise layer
constexpr uint32_t kSaltLuma  = 0x1000193u;
constexpr uint32_t kSaltRed   = 0x8da6b343u;
constexpr uint32_t kSaltGreen = 0xd8163841u;
constexpr uint32_t kSaltBlue  = 0xcb1ab31fu;
constexpr uint32_t kSoftXor   = 0x51633e2du;
constexpr uint32_t kClumpXor  = 0xa511e9b3u;

static float clamp01(float value) {
	return std::max(0.0f, std::min(1.0f, value));
}

static int clampByte(float value) {
	if (value <= 0.0f)
		return 0;
	if (value >= 255.0f)
		return 255;
	return static_cast<int>(value + 0.5f);
}

static float lerp(float a, float b, float t) {
	return a + ((b - a) * t);
}

static uint32_t mix32(uint32_t value) {
	value ^= value >> 16;
	value *= 0x7feb352du;
	value ^= value >> 15;
	value *= 0x846ca68bu;
	value ^= value >> 16;
	return value;
}

static uint32_t hash_string(const std::string& value) {
	uint32_t h = 2166136261u;
	for (unsigned char c : value) {
		h ^= c;
		h *= 16777619u;
	}
	return h;
}

static uint32_t hash_coords(uint32_t seed, int x, int y, int t, uint32_t salt) {
	uint32_t h = seed ^ salt;
	h ^= static_cast<uint32_t>(x) * 0x9e3779b9u;
	h ^= static_cast<uint32_t>(y) * 0x85ebca6bu;
	h ^= static_cast<uint32_t>(t) * 0xc2b2ae35u;
	return mix32(h);
}

static float hash_signed(uint32_t seed, int x, int y, int t, uint32_t salt) {
	return hash_coords(seed, x, y, t, salt) * kInvU32Max - 1.0f;
}

static float tonal_weight(float luma, float shadow_strength, float midtone_strength, float highlight_strength) {
	const float shadow = (1.0f - luma) * (1.0f - luma);
	const float highlight = luma * luma;
	const float centered = std::abs(luma - 0.5f) * 2.0f;
	const float midtone = clamp01(1.0f - centered * centered);
	return (shadow * shadow_strength) + (midtone * midtone_strength) + (highlight * highlight_strength);
}
}

FilmGrain::FilmGrain()
	: amount(0.25),
	  size(0.20),
	  softness(0.25),
	  clump(0.20),
	  shadows(0.80),
	  midtones(1.00),
	  highlights(0.55),
	  color_amount(0.20),
	  color_variation(0.35),
	  evolution(0.65),
	  coherence(0.55),
	  seed(1)
{
	init_effect_details();
}

void FilmGrain::init_effect_details() {
	InitEffectInfo();
	info.class_name = "FilmGrain";
	info.name = "Film Grain";
	info.description = "Film-inspired grain texture with tonal, color, and temporal controls.";
	info.has_audio = false;
	info.has_video = true;
}

std::shared_ptr<openshot::Frame> FilmGrain::GetFrame(std::shared_ptr<openshot::Frame> frame, int64_t frame_number) {
	std::shared_ptr<QImage> frame_image = frame->GetImage();
	if (!frame_image)
		return frame;

	const float amount_value = clamp01(static_cast<float>(amount.GetValue(frame_number)));
	if (amount_value <= 0.00001f)
		return frame;

	const float size_value        = clamp01(static_cast<float>(size.GetValue(frame_number)));
	const float softness_value    = clamp01(static_cast<float>(softness.GetValue(frame_number)));
	const float clump_value       = clamp01(static_cast<float>(clump.GetValue(frame_number)));
	const float shadows_value     = clamp01(static_cast<float>(shadows.GetValue(frame_number)));
	const float midtones_value    = clamp01(static_cast<float>(midtones.GetValue(frame_number)));
	const float highlights_value  = clamp01(static_cast<float>(highlights.GetValue(frame_number)));
	const float color_amount_value    = clamp01(static_cast<float>(color_amount.GetValue(frame_number)));
	const float color_variation_value = clamp01(static_cast<float>(color_variation.GetValue(frame_number)));
	const float evolution_value   = clamp01(static_cast<float>(evolution.GetValue(frame_number)));
	const float coherence_value   = clamp01(static_cast<float>(coherence.GetValue(frame_number)));

	const float cell_size       = lerp(1.0f, 9.0f, size_value);
	const float fine_frequency  = 1.0f / cell_size;
	const float soft_frequency  = fine_frequency * lerp(0.45f, 0.12f, softness_value);
	const float clump_frequency = fine_frequency * lerp(0.35f, 0.08f, clump_value);
	const float temporal_rate   = lerp(0.0f, 1.0f, evolution_value) * lerp(1.0f, 8.0f, 1.0f - coherence_value);
	const float temporal_position   = static_cast<float>(frame_number) * temporal_rate;
	const int   time0               = static_cast<int>(std::floor(temporal_position));
	const int   time1               = time0 + 1;
	const float temporal_mix        = temporal_rate <= 0.00001f ? 0.0f : clamp01(temporal_position - static_cast<float>(time0));
	const float smooth_temporal_mix = temporal_mix * temporal_mix * (3.0f - 2.0f * temporal_mix);
	const uint32_t base_seed        = static_cast<uint32_t>(seed) ^ mix32(hash_string(Id()));
	const float intensity_scale     = amount_value * 0.18f;
	const bool use_temporal_blend   = temporal_rate > 0.00001f && smooth_temporal_mix > 0.00001f;
	const bool use_softness         = softness_value > 0.00001f;
	const bool use_clump            = clump_value > 0.00001f;
	const bool use_color            = color_amount_value > 0.00001f;
	const bool use_color_variation  = use_color && color_variation_value > 0.00001f;

	// How many temporal samples and how many channel salts to compute
	const int num_times = use_temporal_blend ? 2 : 1;
	const int num_salts = use_color_variation ? 4 : 1;
	const int time_buckets[2] = { time0, time1 };
	const uint32_t base_salts[4] = { kSaltLuma, kSaltRed, kSaltGreen, kSaltBlue };

	static const std::array<float, 256> inv_alpha = [] {
		std::array<float, 256> lut{};
		lut[0] = 0.0f;
		for (int i = 1; i < 256; ++i)
			lut[i] = 255.0f / static_cast<float>(i);
		return lut;
	}();

	unsigned char* pixels = reinterpret_cast<unsigned char*>(frame_image->bits());
	const int width  = frame_image->width();
	const int height = frame_image->height();
	const int stride = frame_image->bytesPerLine();
	const int pixel_count = width * height;
	int reference_width  = width;
	int reference_height = height;

	Clip* clip = static_cast<Clip*>(ParentClip());
	Timeline* timeline = nullptr;
	if (clip && clip->ParentTimeline())
		timeline = static_cast<Timeline*>(clip->ParentTimeline());
	else if (ParentTimeline())
		timeline = static_cast<Timeline*>(ParentTimeline());
	if (timeline && timeline->info.width > 0 && timeline->info.height > 0) {
		reference_width  = timeline->info.width;
		reference_height = timeline->info.height;
	}
	const float reference_scale_x = width  > 0 ? static_cast<float>(reference_width)  / static_cast<float>(width)  : 1.0f;
	const float reference_scale_y = height > 0 ? static_cast<float>(reference_height) / static_cast<float>(height) : 1.0f;

	#pragma omp parallel for if(pixel_count >= 16384) schedule(static)
	for (int y = 0; y < height; ++y) {
		unsigned char* row = pixels + (y * stride);

		// Pre-compute all y-dependent values once per row (not per pixel).
		// reference_x/y coords are always non-negative, so (int) cast is safe instead of floor.
		const float reference_y = static_cast<float>(y) * reference_scale_y;
		const int   fine_iy     = static_cast<int>(reference_y * fine_frequency);

		// Soft noise: y-fractional components are constant across the row
		int   soft_iy0      = 0;
		float soft_sy_fac   = 0.0f;  // smoothstep of the y fractional part
		if (use_softness) {
			const float sry  = reference_y * soft_frequency;
			soft_iy0         = static_cast<int>(sry);
			const float t    = sry - static_cast<float>(soft_iy0);
			soft_sy_fac      = t * t * (3.0f - 2.0f * t);
		}

		// Clump noise: same idea
		int   clump_iy0     = 0;
		float clump_sy_fac  = 0.0f;
		if (use_clump) {
			const float cry  = reference_y * clump_frequency;
			clump_iy0        = static_cast<int>(cry);
			const float t    = cry - static_cast<float>(clump_iy0);
			clump_sy_fac     = t * t * (3.0f - 2.0f * t);
		}

		// Per-row x-cell caches. Initialized to -1 so the first pixel always
		// triggers a fill. All computed cell indices are >= 0, so -1 is a safe sentinel.
		int cached_fine_ix    = -1;
		int cached_soft_ix0   = -1;
		int cached_clump_ix0  = -1;

		// fine_cache[time_idx][salt_idx]: direct hash value per fine-grid cell
		float fine_cache[2][4] = {};

		// soft_col0/col1[time_idx][salt_idx]: value_noise y-axis columns pre-lerped.
		// Per pixel we only need one lerp across x instead of 4 hash calls + 3 lerps.
		float soft_col0[2][4] = {}, soft_col1[2][4] = {};

		// clump_col0/col1[salt_idx]: clump only uses time0
		float clump_col0[4] = {}, clump_col1[4] = {};

		for (int x = 0; x < width; ++x) {
			const int idx = x * 4;
			const int A   = row[idx + 3];
			if (A <= 0)
				continue;

			float R, G, B;
			if (A == 255) {
				R = row[idx + 0] * kInv255;
				G = row[idx + 1] * kInv255;
				B = row[idx + 2] * kInv255;
			} else {
				const float inv_a = inv_alpha[A];
				R = (row[idx + 0] * inv_a) * kInv255;
				G = (row[idx + 1] * inv_a) * kInv255;
				B = (row[idx + 2] * inv_a) * kInv255;
			}

			const float luma = (0.299f * R) + (0.587f * G) + (0.114f * B);
			const float tone = tonal_weight(luma, shadows_value, midtones_value, highlights_value);
			if (tone <= 0.00001f)
				continue;

			const float reference_x = static_cast<float>(x) * reference_scale_x;

			// Fine grain: one hash per (cell, time, salt). Update when the x-cell changes.
			const int fine_ix = static_cast<int>(reference_x * fine_frequency);
			if (fine_ix != cached_fine_ix) {
				cached_fine_ix = fine_ix;
				for (int t = 0; t < num_times; ++t)
					for (int s = 0; s < num_salts; ++s)
						fine_cache[t][s] = hash_signed(base_seed, fine_ix, fine_iy, time_buckets[t], base_salts[s]);
			}

			// Soft noise: bilinear value_noise, but y-axis columns are pre-lerped into
			// soft_col0/col1. Only 4 hashes needed when the x-cell changes (~every 7 px),
			// then 1 lerp per pixel instead of 4 hashes + 3 lerps every pixel.
			float soft_sx = 0.0f;
			if (use_softness) {
				const float srx    = reference_x * soft_frequency;
				const int soft_ix0 = static_cast<int>(srx);
				if (soft_ix0 != cached_soft_ix0) {
					cached_soft_ix0    = soft_ix0;
					const int soft_ix1 = soft_ix0 + 1;
					const int soft_iy1 = soft_iy0 + 1;
					for (int t = 0; t < num_times; ++t) {
						for (int s = 0; s < num_salts; ++s) {
							const uint32_t salt = base_salts[s] ^ kSoftXor;
							const float n00 = hash_signed(base_seed, soft_ix0, soft_iy0, time_buckets[t], salt);
							const float n10 = hash_signed(base_seed, soft_ix1, soft_iy0, time_buckets[t], salt);
							const float n01 = hash_signed(base_seed, soft_ix0, soft_iy1, time_buckets[t], salt);
							const float n11 = hash_signed(base_seed, soft_ix1, soft_iy1, time_buckets[t], salt);
							// y-interpolate left and right columns separately; x-interpolation is per-pixel
							soft_col0[t][s] = n00 + (n01 - n00) * soft_sy_fac;
							soft_col1[t][s] = n10 + (n11 - n10) * soft_sy_fac;
						}
					}
				}
				const float tx = srx - static_cast<float>(cached_soft_ix0);
				soft_sx = tx * tx * (3.0f - 2.0f * tx);
			}

			// Clump noise: same column-caching strategy, only time0
			float clump_sx = 0.0f;
			if (use_clump) {
				const float crx      = reference_x * clump_frequency;
				const int clump_ix0  = static_cast<int>(crx);
				if (clump_ix0 != cached_clump_ix0) {
					cached_clump_ix0   = clump_ix0;
					const int clump_ix1 = clump_ix0 + 1;
					const int clump_iy1 = clump_iy0 + 1;
					for (int s = 0; s < num_salts; ++s) {
						const uint32_t salt = base_salts[s] ^ kClumpXor;
						const float n00 = hash_signed(base_seed, clump_ix0, clump_iy0, time0, salt);
						const float n10 = hash_signed(base_seed, clump_ix1, clump_iy0, time0, salt);
						const float n01 = hash_signed(base_seed, clump_ix0, clump_iy1, time0, salt);
						const float n11 = hash_signed(base_seed, clump_ix1, clump_iy1, time0, salt);
						clump_col0[s] = n00 + (n01 - n00) * clump_sy_fac;
						clump_col1[s] = n10 + (n11 - n10) * clump_sy_fac;
					}
				}
				const float tx = crx - static_cast<float>(cached_clump_ix0);
				clump_sx = tx * tx * (3.0f - 2.0f * tx);
			}

			float grains[4];
			for (int s = 0; s < num_salts; ++s) {
				float g0 = fine_cache[0][s];
				if (use_softness) {
					const float soft0 = soft_col0[0][s] + (soft_col1[0][s] - soft_col0[0][s]) * soft_sx;
					g0 += (soft0 - g0) * softness_value;
				}
				float grain;
				if (use_temporal_blend) {
					float g1 = fine_cache[1][s];
					if (use_softness) {
						const float soft1 = soft_col0[1][s] + (soft_col1[1][s] - soft_col0[1][s]) * soft_sx;
						g1 += (soft1 - g1) * softness_value;
					}
					grain = g0 + (g1 - g0) * smooth_temporal_mix;
				} else {
					grain = g0;
				}
				if (use_clump) {
					const float cluster = clump_col0[s] + (clump_col1[s] - clump_col0[s]) * clump_sx;
					grain *= lerp(1.0f, 0.45f + (std::abs(cluster) * 1.35f), clump_value);
				}
				grains[s] = grain;
			}

			const float strength   = intensity_scale * tone;
			const float luma_grain = grains[0];
			if (use_color_variation) {
				const float red_grain   = luma_grain + (grains[1] - luma_grain) * color_variation_value;
				const float green_grain = luma_grain + (grains[2] - luma_grain) * color_variation_value;
				const float blue_grain  = luma_grain + (grains[3] - luma_grain) * color_variation_value;
				R = clamp01(R + (luma_grain + (red_grain   - luma_grain) * color_amount_value) * strength);
				G = clamp01(G + (luma_grain + (green_grain - luma_grain) * color_amount_value) * strength);
				B = clamp01(B + (luma_grain + (blue_grain  - luma_grain) * color_amount_value) * strength);
			} else {
				const float delta = luma_grain * strength;
				R = clamp01(R + delta);
				G = clamp01(G + delta);
				B = clamp01(B + delta);
			}

			if (A == 255) {
				row[idx + 0] = static_cast<unsigned char>(clampByte(R * 255.0f));
				row[idx + 1] = static_cast<unsigned char>(clampByte(G * 255.0f));
				row[idx + 2] = static_cast<unsigned char>(clampByte(B * 255.0f));
			} else {
				const float alpha_percent = static_cast<float>(A) * kInv255;
				row[idx + 0] = static_cast<unsigned char>(clampByte(R * 255.0f * alpha_percent));
				row[idx + 1] = static_cast<unsigned char>(clampByte(G * 255.0f * alpha_percent));
				row[idx + 2] = static_cast<unsigned char>(clampByte(B * 255.0f * alpha_percent));
			}
		}
	}

	return frame;
}

std::string FilmGrain::Json() const {
	return JsonValue().toStyledString();
}

Json::Value FilmGrain::JsonValue() const {
	Json::Value root = EffectBase::JsonValue();
	root["type"]            = info.class_name;
	root["amount"]          = amount.JsonValue();
	root["size"]            = size.JsonValue();
	root["softness"]        = softness.JsonValue();
	root["clump"]           = clump.JsonValue();
	root["shadows"]         = shadows.JsonValue();
	root["midtones"]        = midtones.JsonValue();
	root["highlights"]      = highlights.JsonValue();
	root["color_amount"]    = color_amount.JsonValue();
	root["color_variation"] = color_variation.JsonValue();
	root["evolution"]       = evolution.JsonValue();
	root["coherence"]       = coherence.JsonValue();
	root["seed"]            = seed;
	return root;
}

void FilmGrain::SetJson(const std::string value) {
	try {
		const Json::Value root = openshot::stringToJson(value);
		SetJsonValue(root);
	} catch (const std::exception&) {
		throw InvalidJSON("Invalid JSON for FilmGrain effect");
	}
}

void FilmGrain::SetJsonValue(const Json::Value root) {
	EffectBase::SetJsonValue(root);
	if (!root["amount"].isNull())          amount.SetJsonValue(root["amount"]);
	if (!root["size"].isNull())            size.SetJsonValue(root["size"]);
	if (!root["softness"].isNull())        softness.SetJsonValue(root["softness"]);
	if (!root["clump"].isNull())           clump.SetJsonValue(root["clump"]);
	if (!root["shadows"].isNull())         shadows.SetJsonValue(root["shadows"]);
	if (!root["midtones"].isNull())        midtones.SetJsonValue(root["midtones"]);
	if (!root["highlights"].isNull())      highlights.SetJsonValue(root["highlights"]);
	if (!root["color_amount"].isNull())    color_amount.SetJsonValue(root["color_amount"]);
	if (!root["color_variation"].isNull()) color_variation.SetJsonValue(root["color_variation"]);
	if (!root["evolution"].isNull())       evolution.SetJsonValue(root["evolution"]);
	if (!root["coherence"].isNull())       coherence.SetJsonValue(root["coherence"]);
	if (!root["seed"].isNull())            seed = root["seed"].asInt();
}

std::string FilmGrain::PropertiesJSON(int64_t requested_frame) const {
	Json::Value root = BasePropertiesJSON(requested_frame);
	root["amount"]          = add_property_json("Amount",          amount.GetValue(requested_frame),          "float", "Overall grain intensity.",                               &amount,          0.0, 1.0, false, requested_frame);
	root["size"]            = add_property_json("Size",            size.GetValue(requested_frame),            "float", "Fine to coarse grain scale.",                            &size,            0.0, 1.0, false, requested_frame);
	root["softness"]        = add_property_json("Softness",        softness.GetValue(requested_frame),        "float", "Hard crisp grain to softer organic grain.",              &softness,        0.0, 1.0, false, requested_frame);
	root["clump"]           = add_property_json("Clump",           clump.GetValue(requested_frame),           "float", "Even grain to clustered irregular grain.",               &clump,           0.0, 1.0, false, requested_frame);
	root["shadows"]         = add_property_json("Shadows",         shadows.GetValue(requested_frame),         "float", "Grain strength in dark regions.",                        &shadows,         0.0, 1.0, false, requested_frame);
	root["midtones"]        = add_property_json("Midtones",        midtones.GetValue(requested_frame),        "float", "Grain strength in middle tonal regions.",                &midtones,        0.0, 1.0, false, requested_frame);
	root["highlights"]      = add_property_json("Highlights",      highlights.GetValue(requested_frame),      "float", "Grain strength in bright regions.",                      &highlights,      0.0, 1.0, false, requested_frame);
	root["color_amount"]    = add_property_json("Color Amount",    color_amount.GetValue(requested_frame),    "float", "How much grain affects chroma instead of mostly luma.",  &color_amount,    0.0, 1.0, false, requested_frame);
	root["color_variation"] = add_property_json("Color Variation", color_variation.GetValue(requested_frame), "float", "Correlated to independently varied color grain.",        &color_variation, 0.0, 1.0, false, requested_frame);
	root["evolution"]       = add_property_json("Evolution",       evolution.GetValue(requested_frame),       "float", "How much grain renews over time.",                       &evolution,       0.0, 1.0, false, requested_frame);
	root["coherence"]       = add_property_json("Coherence",       coherence.GetValue(requested_frame),       "float", "How stable and smooth grain remains between frames.",    &coherence,       0.0, 1.0, false, requested_frame);
	root["seed"]            = add_property_json("Seed",            seed,                                      "int",   "Deterministic grain variation.",                         NULL,             0,   1000000, false, requested_frame);
	return root.toStyledString();
}
