/**
 * @file
 * @brief Source file for AnalogTape effect class
 * @author Jonathan Thomas <jonathan@openshot.org>
 *
 * @ref License
 */

// Copyright (c) 2008-2025 OpenShot Studios, LLC
//
// SPDX-License-Identifier: LGPL-3.0-or-later

#include "AnalogTape.h"
#include "Clip.h"
#include "Exceptions.h"
#include "ReaderBase.h"
#include "Timeline.h"

#include <algorithm>
#include <cmath>

using namespace openshot;

AnalogTape::AnalogTape()
		: tracking(0.55), bleed(0.65), softness(0.40), noise(0.50), stripe(0.25f),
			staticBands(0.20f), seed_offset(0) {
	init_effect_details();
}

AnalogTape::AnalogTape(Keyframe t, Keyframe b, Keyframe s, Keyframe n,
											 Keyframe st, Keyframe sb, int seed)
		: tracking(t), bleed(b), softness(s), noise(n), stripe(st),
			staticBands(sb), seed_offset(seed) {
	init_effect_details();
}

void AnalogTape::init_effect_details() {
	InitEffectInfo();
	info.class_name = "AnalogTape";
	info.name = "Analog Tape";
	info.description = "Vintage home video wobble, bleed, and grain.";
	info.has_video = true;
	info.has_audio = false;
}

static inline float lerp(float a, float b, float t) { return a + (b - a) * t; }

std::shared_ptr<Frame> AnalogTape::GetFrame(std::shared_ptr<Frame> frame,
																						int64_t frame_number) {
	std::shared_ptr<QImage> img = frame->GetImage();
	int w = img->width();
	int h = img->height();
	int Uw = (w + 1) / 2;
	int stride = img->bytesPerLine() / 4;
	uint32_t *base = reinterpret_cast<uint32_t *>(img->bits());

	if (w != last_w || h != last_h) {
		last_w = w;
		last_h = h;
		Y.resize(w * h);
		U.resize(Uw * h);
		V.resize(Uw * h);
		tmpY.resize(w * h);
		tmpU.resize(Uw * h);
		tmpV.resize(Uw * h);
		dx.resize(h);
	}


#ifdef _OPENMP
#pragma omp parallel for
#endif
	for (int y = 0; y < h; ++y) {
		uint32_t *row = base + y * stride;
		float *yrow = &Y[y * w];
		float *urow = &U[y * Uw];
		float *vrow = &V[y * Uw];
		for (int x2 = 0; x2 < Uw; ++x2) {
			int x0 = x2 * 2;
			uint32_t p0 = row[x0];
			float r0 = ((p0 >> 16) & 0xFF) / 255.0f;
			float g0 = ((p0 >> 8) & 0xFF) / 255.0f;
			float b0 = (p0 & 0xFF) / 255.0f;
			float y0 = 0.299f * r0 + 0.587f * g0 + 0.114f * b0;
			float u0 = -0.14713f * r0 - 0.28886f * g0 + 0.436f * b0;
			float v0 = 0.615f * r0 - 0.51499f * g0 - 0.10001f * b0;
			yrow[x0] = y0;

			float u, v;
			if (x0 + 1 < w) {
				uint32_t p1 = row[x0 + 1];
				float r1 = ((p1 >> 16) & 0xFF) / 255.0f;
				float g1 = ((p1 >> 8) & 0xFF) / 255.0f;
				float b1 = (p1 & 0xFF) / 255.0f;
				float y1 = 0.299f * r1 + 0.587f * g1 + 0.114f * b1;
				float u1 = -0.14713f * r1 - 0.28886f * g1 + 0.436f * b1;
				float v1 = 0.615f * r1 - 0.51499f * g1 - 0.10001f * b1;
				yrow[x0 + 1] = y1;
				u = (u0 + u1) * 0.5f;
				v = (v0 + v1) * 0.5f;
			} else {
				u = u0;
				v = v0;
			}
			urow[x2] = u;
			vrow[x2] = v;
		}
	}

	Fraction fps(1, 1);
	Clip *clip = (Clip *)ParentClip();
	Timeline *timeline = nullptr;
	if (clip && clip->ParentTimeline())
		timeline = (Timeline *)clip->ParentTimeline();
	else if (ParentTimeline())
		timeline = (Timeline *)ParentTimeline();
	if (timeline)
		fps = timeline->info.fps;
	else if (clip && clip->Reader())
		fps = clip->Reader()->info.fps;
	double fps_d = fps.ToDouble();
	double t = fps_d > 0 ? frame_number / fps_d : frame_number;

	const float k_track = tracking.GetValue(frame_number);
	const float k_bleed = bleed.GetValue(frame_number);
	const float k_soft = softness.GetValue(frame_number);
	const float k_noise = noise.GetValue(frame_number);
	const float k_stripe = stripe.GetValue(frame_number);
	const float k_bands = staticBands.GetValue(frame_number);

	int r_y = std::round(lerp(0.0f, 2.0f, k_soft));
	if (k_noise > 0.6f)
		r_y = std::min(r_y, 1);
	if (r_y > 0) {
#ifdef _OPENMP
#pragma omp parallel for
#endif
		for (int y = 0; y < h; ++y)
			box_blur_row(&Y[y * w], &tmpY[y * w], w, r_y);
		Y.swap(tmpY);
	}

	float shift = lerp(0.0f, 2.5f, k_bleed);
	int r_c = std::round(lerp(0.0f, 3.0f, k_bleed));
	float sat = 1.0f - 0.30f * k_bleed;
	float shift_h = shift * 0.5f;
#ifdef _OPENMP
#pragma omp parallel for
#endif
	for (int y = 0; y < h; ++y) {
		const float *srcU = &U[y * Uw];
		const float *srcV = &V[y * Uw];
		float *dstU = &tmpU[y * Uw];
		float *dstV = &tmpV[y * Uw];
		for (int x = 0; x < Uw; ++x) {
			float xs = std::clamp(x - shift_h, 0.0f, float(Uw - 1));
			int x0 = int(xs);
			int x1 = std::min(x0 + 1, Uw - 1);
			float t = xs - x0;
			dstU[x] = srcU[x0] * (1 - t) + srcU[x1] * t;
			dstV[x] = srcV[x0] * (1 - t) + srcV[x1] * t;
		}
	}
	U.swap(tmpU);
	V.swap(tmpV);

	if (r_c > 0) {
#ifdef _OPENMP
#pragma omp parallel for
#endif
		for (int y = 0; y < h; ++y)
			box_blur_row(&U[y * Uw], &tmpU[y * Uw], Uw, r_c);
		U.swap(tmpU);
#ifdef _OPENMP
#pragma omp parallel for
#endif
		for (int y = 0; y < h; ++y)
			box_blur_row(&V[y * Uw], &tmpV[y * Uw], Uw, r_c);
		V.swap(tmpV);
	}

	uint32_t SEED = fnv1a_32(Id()) ^ (uint32_t)seed_offset;
		uint32_t schedSalt = (uint32_t)(k_bands * 64.0f) ^
												 ((uint32_t)(k_stripe * 64.0f) << 8) ^
												 ((uint32_t)(k_noise * 64.0f) << 16);
	uint32_t SCHED_SEED = SEED ^ fnv1a_32(schedSalt, 0x9e3779b9u);
	const float PI = 3.14159265358979323846f;

	float sigmaY = lerp(0.0f, 0.08f, k_noise);
	const float decay = 0.88f + 0.08f * k_noise;
	const float amp = 0.18f * k_noise;
	const float baseP = 0.0025f + 0.02f * k_noise;

	float Hfixed = lerp(0.0f, 0.12f * h, k_stripe);
	float Gfixed = 0.10f * k_stripe;
	float Nfixed = 1.0f + 1.5f * k_stripe;

	float rate = 0.4f * k_bands;
	int dur_frames = std::round(lerp(1.0f, 6.0f, k_bands));
	float Hburst = lerp(0.06f * h, 0.25f * h, k_bands);
	float Gburst = lerp(0.10f, 0.25f, k_bands);
	float sat_band = lerp(0.8f, 0.5f, k_bands);
	float Nburst = 1.0f + 2.0f * k_bands;

	struct Band { float center; double t0; };
	std::vector<Band> bands;
	if (k_bands > 0.0f && rate > 0.0f) {
		const double win_len = 0.25;
		int win_idx = int(t / win_len);
		double lambda = rate * win_len *
										(0.25 + 1.5f * row_density(SCHED_SEED, frame_number, 0));
		double prob_ge1 = 1.0 - std::exp(-lambda);
		double prob_ge2 = 1.0 - std::exp(-lambda) - lambda * std::exp(-lambda);

		auto spawn_band = [&](int kseed) {
			float r1 = hash01(SCHED_SEED, uint32_t(win_idx), 11 + kseed, 0);
			float start = r1 * win_len;
			float center =
					hash01(SCHED_SEED, uint32_t(win_idx), 12 + kseed, 0) * (h - Hburst) +
					0.5f * Hburst;
			double t0 = win_idx * win_len + start;
			double t1 = t0 + dur_frames / (fps_d > 0 ? fps_d : 1.0);
			if (t >= t0 && t < t1)
				bands.push_back({center, t0});
		};

		float r = hash01(SCHED_SEED, uint32_t(win_idx), 9, 0);
		if (r < prob_ge1)
			spawn_band(0);
		if (r < prob_ge2)
			spawn_band(1);
	}

	float ft = 2.0f;
	int kf = int(std::floor(t * ft));
	float a = float(t * ft - kf);

#ifdef _OPENMP
#pragma omp parallel for
#endif
	for (int y = 0; y < h; ++y) {
		float bandF = 0.0f;
		if (Hfixed > 0.0f && y >= h - Hfixed)
			bandF = (y - (h - Hfixed)) / std::max(1.0f, Hfixed);
		float burstF = 0.0f;
		for (const auto &b : bands) {
			float halfH = Hburst * 0.5f;
			float dist = std::abs(y - b.center);
			float profile = std::max(0.0f, 1.0f - dist / halfH);
			float life = float((t - b.t0) * fps_d);
			float env = (life < 1.0f)
											? life
											: (life < dur_frames - 1 ? 1.0f
																							: std::max(0.0f, dur_frames - life));
			burstF = std::max(burstF, profile * env);
		}

		float sat_row = 1.0f - (1.0f - sat_band) * burstF;
		if (burstF > 0.0f && sat_row != 1.0f) {
			float *urow = &U[y * Uw];
			float *vrow = &V[y * Uw];
			for (int xh = 0; xh < Uw; ++xh) {
				urow[xh] *= sat_row;
				vrow[xh] *= sat_row;
			}
		}

		float rowBias = row_density(SEED, frame_number, y);
		float p = baseP * (0.25f + 1.5f * rowBias);
		p *= (1.0f + 1.5f * bandF + 2.0f * burstF);

		float hum = 0.008f * k_noise *
								std::sin(2 * PI * (y * (6.0f / h) + 0.08f * t));
		uint32_t s0 = SEED ^ 0x9e37u * kf ^ 0x85ebu * y;
		uint32_t s1 = SEED ^ 0x9e37u * (kf + 1) ^ 0x85ebu * y ^ 0x1234567u;
		auto step = [](uint32_t &s) {
			s ^= s << 13;
			s ^= s >> 17;
			s ^= s << 5;
			return s;
		};
		float lift = Gfixed * bandF + Gburst * burstF;
		float rowSigma = sigmaY * (1 + (Nfixed - 1) * bandF +
															(Nburst - 1) * burstF);
		float k = 0.15f + 0.35f * hash01(SEED, uint32_t(frame_number), y, 777);
		float sL = 0.0f, sR = 0.0f;
		for (int x = 0; x < w; ++x) {
			if (hash01(SEED, uint32_t(frame_number), y, x) < p)
				sL = 1.0f;
			if (hash01(SEED, uint32_t(frame_number), y, w - 1 - x) < p * 0.7f)
				sR = 1.0f;
			float n = ((step(s0) & 0xFFFFFF) / 16777215.0f) * (1 - a) +
								((step(s1) & 0xFFFFFF) / 16777215.0f) * a;
			int idx = y * w + x;
			float mt = std::clamp((Y[idx] - 0.2f) / (0.8f - 0.2f), 0.0f, 1.0f);
			float val = Y[idx] + lift + rowSigma * (2 * n - 1) *
															 (0.6f + 0.4f * mt) + hum;
			float streak = amp * (sL + sR);
			float newY = val + streak * (k + (1.0f - val));
			Y[idx] = std::clamp(newY, 0.0f, 1.0f);
			sL *= decay;
			sR *= decay;
		}
	}

	float A = lerp(0.0f, 3.0f, k_track); // pixels
	float f = lerp(0.25f, 1.2f, k_track); // Hz
	float Hsk = lerp(0.0f, 0.10f * h, k_track); // pixels
	float S = lerp(0.0f, 5.0f, k_track); // pixels
	float phase = 2 * PI * (f * t) + 0.7f * (SEED * 0.001f);
	for (int y = 0; y < h; ++y) {
		float base = A * std::sin(2 * PI * 0.0035f * y + phase);
		float skew = (y >= h - Hsk)
										 ? S * ((y - (h - Hsk)) / std::max(1.0f, Hsk))
										 : 0.0f;
		dx[y] = base + skew;
	}

	auto remap_line = [&](const float *src, float *dst, int width, float scale) {
#ifdef _OPENMP
#pragma omp parallel for
#endif
		for (int y = 0; y < h; ++y) {
			float off = dx[y] * scale;
			const float *s = src + y * width;
			float *d = dst + y * width;
			int start = std::max(0, int(std::ceil(-off)));
			int end = std::min(width, int(std::floor(width - off)));
			float xs = start + off;
			int x0 = int(xs);
			float t = xs - x0;
			for (int x = start; x < end; ++x) {
				int x1 = x0 + 1;
				d[x] = s[x0] * (1 - t) + s[x1] * t;
				xs += 1.0f;
				x0 = int(xs);
				t = xs - x0;
			}
			for (int x = 0; x < start; ++x)
				d[x] = s[0];
			for (int x = end; x < width; ++x)
				d[x] = s[width - 1];
		}
	};

	remap_line(Y.data(), tmpY.data(), w, 1.0f);
	Y.swap(tmpY);
	remap_line(U.data(), tmpU.data(), Uw, 0.5f);
	U.swap(tmpU);
	remap_line(V.data(), tmpV.data(), Uw, 0.5f);
	V.swap(tmpV);

#ifdef _OPENMP
#pragma omp parallel for
#endif
	for (int y = 0; y < h; ++y) {
		float *yrow = &Y[y * w];
		float *urow = &U[y * Uw];
		float *vrow = &V[y * Uw];
		uint32_t *row = base + y * stride;
		for (int x = 0; x < w; ++x) {
			float xs = x * 0.5f;
			int x0 = int(xs);
			int x1 = std::min(x0 + 1, Uw - 1);
			float t = xs - x0;
			float u = (urow[x0] * (1 - t) + urow[x1] * t) * sat;
			float v = (vrow[x0] * (1 - t) + vrow[x1] * t) * sat;
			float yv = yrow[x];
			float r = yv + 1.13983f * v;
			float g = yv - 0.39465f * u - 0.58060f * v;
			float b = yv + 2.03211f * u;
			int R = int(std::clamp(r, 0.0f, 1.0f) * 255.0f);
			int G = int(std::clamp(g, 0.0f, 1.0f) * 255.0f);
			int B = int(std::clamp(b, 0.0f, 1.0f) * 255.0f);
			uint32_t A = row[x] & 0xFF000000u;
			row[x] = A | (R << 16) | (G << 8) | B;
		}
	}

	return frame;
}

// JSON
std::string AnalogTape::Json() const { return JsonValue().toStyledString(); }

Json::Value AnalogTape::JsonValue() const {
	Json::Value root = EffectBase::JsonValue();
	root["type"] = info.class_name;
	root["tracking"] = tracking.JsonValue();
	root["bleed"] = bleed.JsonValue();
	root["softness"] = softness.JsonValue();
	root["noise"] = noise.JsonValue();
	root["stripe"] = stripe.JsonValue();
	root["static_bands"] = staticBands.JsonValue();
	root["seed_offset"] = seed_offset;
	return root;
}

void AnalogTape::SetJson(const std::string value) {
	try {
		Json::Value root = openshot::stringToJson(value);
		SetJsonValue(root);
	} catch (const std::exception &) {
		throw InvalidJSON("JSON is invalid (missing keys or invalid data types)");
	}
}

void AnalogTape::SetJsonValue(const Json::Value root) {
	EffectBase::SetJsonValue(root);
	if (!root["tracking"].isNull())
		tracking.SetJsonValue(root["tracking"]);
	if (!root["bleed"].isNull())
		bleed.SetJsonValue(root["bleed"]);
	if (!root["softness"].isNull())
		softness.SetJsonValue(root["softness"]);
	if (!root["noise"].isNull())
		noise.SetJsonValue(root["noise"]);
	if (!root["stripe"].isNull())
		stripe.SetJsonValue(root["stripe"]);
	if (!root["static_bands"].isNull())
		staticBands.SetJsonValue(root["static_bands"]);
	if (!root["seed_offset"].isNull())
		seed_offset = root["seed_offset"].asInt();
}

std::string AnalogTape::PropertiesJSON(int64_t requested_frame) const {
	Json::Value root = BasePropertiesJSON(requested_frame);
	root["tracking"] =
			add_property_json("Tracking", tracking.GetValue(requested_frame), "float",
												"", &tracking, 0, 1, false, requested_frame);
	root["bleed"] =
			add_property_json("Bleed", bleed.GetValue(requested_frame), "float", "",
												&bleed, 0, 1, false, requested_frame);
	root["softness"] =
			add_property_json("Softness", softness.GetValue(requested_frame), "float",
												"", &softness, 0, 1, false, requested_frame);
	root["noise"] =
			add_property_json("Noise", noise.GetValue(requested_frame), "float", "",
												&noise, 0, 1, false, requested_frame);
	root["stripe"] =
			add_property_json("Stripe", stripe.GetValue(requested_frame), "float",
												"Bottom tracking stripe brightness and noise.",
												&stripe, 0, 1, false, requested_frame);
	root["static_bands"] =
			add_property_json("Static Bands", staticBands.GetValue(requested_frame),
												"float",
												"Short bright static bands and extra dropouts.",
												&staticBands, 0, 1, false, requested_frame);
	root["seed_offset"] =
			add_property_json("Seed Offset", seed_offset, "int", "", NULL, 0, 1000,
												false, requested_frame);
	return root.toStyledString();
}
