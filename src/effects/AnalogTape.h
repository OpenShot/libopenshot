/**
 * @file
 * @brief Header file for AnalogTape effect class
 *
 * Vintage home video wobble, bleed, and grain.
 *
 * @author Jonathan Thomas <jonathan@openshot.org>
 *
 * @ref License
 */

// Copyright (c) 2008-2025 OpenShot Studios, LLC
//
// SPDX-License-Identifier: LGPL-3.0-or-later

#ifndef OPENSHOT_ANALOGTAPE_EFFECT_H
#define OPENSHOT_ANALOGTAPE_EFFECT_H

#include "../EffectBase.h"
#include "../Frame.h"
#include "../Json.h"
#include "../KeyFrame.h"

#include <algorithm>
#include <cmath>
#include <cstring>
#include <cstdint>
#include <memory>
#include <string>
#include <vector>

#if defined(__GNUC__) || defined(__clang__)
#define OS_RESTRICT __restrict__
#else
#define OS_RESTRICT
#endif

namespace openshot {

/// Analog video tape simulation effect.
class AnalogTape : public EffectBase {
private:
	void init_effect_details();
	static inline uint32_t fnv1a_32(const std::string &s) {
		uint32_t h = 2166136261u;
		for (unsigned char c : s) {
			h ^= c;
			h *= 16777619u;
		}
		return h;
	}
	static inline uint32_t fnv1a_32(uint32_t h, uint32_t d) {
		unsigned char bytes[4];
		bytes[0] = d & 0xFF;
		bytes[1] = (d >> 8) & 0xFF;
		bytes[2] = (d >> 16) & 0xFF;
		bytes[3] = (d >> 24) & 0xFF;
		for (int i = 0; i < 4; ++i) {
			h ^= bytes[i];
			h *= 16777619u;
		}
		return h;
	}
	static inline float hash01(uint32_t seed, uint32_t a, uint32_t b, uint32_t c) {
		uint32_t h = fnv1a_32(seed, a);
		h = fnv1a_32(h, b);
		h = fnv1a_32(h, c);
		return h / 4294967295.0f;
	}
	static inline float row_density(uint32_t seed, int frame, int y) {
		int tc = (frame >> 3);
		int y0 = (y >> 3);
		float a = (y & 7) / 8.0f;
		float h0 = hash01(seed, tc, y0, 31);
		float h1 = hash01(seed, tc, y0 + 1, 31);
		float m = (1 - a) * h0 + a * h1;
		return m * m;
	}
	static inline void box_blur_row(const float *OS_RESTRICT src,
																	float *OS_RESTRICT dst, int w, int r) {
		if (r == 0) {
			std::memcpy(dst, src, w * sizeof(float));
			return;
		}
		const int win = 2 * r + 1;
		float sum = 0.0f;
		for (int k = -r; k <= r; ++k)
			sum += src[std::clamp(k, 0, w - 1)];
		dst[0] = sum / win;
		for (int x = 1; x < w; ++x) {
			int add = std::min(w - 1, x + r);
			int sub = std::max(0, x - r - 1);
			sum += src[add] - src[sub];
			dst[x] = sum / win;
		}
	}

	int last_w = 0, last_h = 0;
	std::vector<float> Y, U, V, tmpY, tmpU, tmpV, dx;

public:
	Keyframe tracking; ///< tracking wobble amount
	Keyframe bleed;    ///< color bleed amount
	Keyframe softness; ///< luma blur radius
	Keyframe noise;    ///< grain/dropouts amount
	Keyframe stripe;   ///< bottom tracking stripe strength
	Keyframe staticBands; ///< burst static band strength
	int seed_offset;   ///< seed offset for deterministic randomness

	AnalogTape();
	AnalogTape(Keyframe tracking, Keyframe bleed, Keyframe softness,
						 Keyframe noise, Keyframe stripe, Keyframe staticBands,
						 int seed_offset = 0);

	std::shared_ptr<openshot::Frame>
	GetFrame(std::shared_ptr<openshot::Frame> frame,
					 int64_t frame_number) override;

	std::shared_ptr<openshot::Frame> GetFrame(int64_t frame_number) override {
		return GetFrame(std::make_shared<openshot::Frame>(), frame_number);
	}

	// JSON
	std::string Json() const override;
	void SetJson(const std::string value) override;
	Json::Value JsonValue() const override;
	void SetJsonValue(const Json::Value root) override;

std::string PropertiesJSON(int64_t requested_frame) const override;
};

} // namespace openshot

#undef OS_RESTRICT

#endif
