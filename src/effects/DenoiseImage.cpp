/**
 * @file
 * @brief Source file for DenoiseImage effect
 * @author Jonathan Thomas <jonathan@openshot.org>
 *
 * @ref License
 */

// Copyright (c) 2008-2026 OpenShot Studios, LLC
//
// SPDX-License-Identifier: LGPL-3.0-or-later

#include "DenoiseImage.h"
#include "Exceptions.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <memory>

using namespace openshot;

namespace {
constexpr float kInv255 = 1.0f / 255.0f;

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

static inline float luma(float r, float g, float b) {
	return (0.299f * r) + (0.587f * g) + (0.114f * b);
}

static inline float smoothstep(float edge0, float edge1, float value) {
	if (edge0 == edge1)
		return value < edge0 ? 0.0f : 1.0f;
	const float t = clamp01((value - edge0) / (edge1 - edge0));
	return t * t * (3.0f - (2.0f * t));
}

static inline float lerp(float a, float b, float amount) {
	return a + ((b - a) * amount);
}

static std::array<float, 256> build_response_lut(const AnimatedCurve& curve, int64_t frame_number) {
	std::array<float, 256> lut{};
	if (curve.enabled.GetValue(frame_number) < 0.5) {
		lut.fill(1.0f);
		return lut;
	}

	const Keyframe sampled_curve = curve.BuildCurve(frame_number, 255.0);
	for (size_t i = 0; i < lut.size(); ++i)
		lut[i] = clamp01(static_cast<float>(sampled_curve.GetValue(static_cast<int64_t>(i))));
	return lut;
}

struct DenoisePixelLut {
	std::array<float, 256> fine_threshold{};
	std::array<float, 256> mid_threshold{};
	std::array<float, 256> fine_keep{};
	std::array<float, 256> mid_keep{};
	std::array<float, 256> chroma_fine_threshold_rb{};
	std::array<float, 256> chroma_fine_threshold_g{};
	std::array<float, 256> chroma_mid_threshold{};
	std::array<float, 256> chroma_fine_keep{};
	std::array<float, 256> chroma_mid_keep{};
	std::array<float, 256> temporal_mix_base{};
};

static DenoisePixelLut build_pixel_lut(const std::array<float, 256>& response_lut,
                                       float strength_value, float detail_value,
                                       float temporal_value, float color_noise_value) {
	DenoisePixelLut lut;
	const float strength_overdrive = strength_value * (1.0f + (0.85f * strength_value * strength_value));
	const float detail_structural_keep = detail_value * (0.62f + (0.34f * detail_value));
	const float detail_fine_keep = detail_value * detail_value * detail_value * 0.35f;
	const float inverse_detail = 1.0f - detail_value;
	const float fine_threshold_scale = 24.0f + (inverse_detail * 42.0f);
	const float mid_threshold_scale = 9.0f + (inverse_detail * 20.0f);
	const float fine_keep_scale = 0.95f + (inverse_detail * 0.24f);
	const float mid_keep_scale = 0.46f + (inverse_detail * 0.22f);
	const float chroma_scale_rb = 0.75f + (color_noise_value * 0.55f);
	const float chroma_scale_g = 0.80f + (color_noise_value * 0.45f);
	const float chroma_mid_scale = 0.85f + (color_noise_value * 0.45f);
	const float temporal_scale = temporal_value * strength_value;

	for (size_t i = 0; i < response_lut.size(); ++i) {
		const float response = response_lut[i];
		const float noise_amount = strength_overdrive * response;
		const float fine_threshold = 4.0f + (noise_amount * fine_threshold_scale);
		const float mid_threshold = 2.0f + (noise_amount * mid_threshold_scale);
		const float fine_keep = std::max(detail_fine_keep, 1.0f - (noise_amount * fine_keep_scale));
		const float mid_keep = std::max(detail_structural_keep, 1.0f - (noise_amount * mid_keep_scale));

		lut.fine_threshold[i] = fine_threshold;
		lut.mid_threshold[i] = mid_threshold;
		lut.fine_keep[i] = fine_keep;
		lut.mid_keep[i] = mid_keep;
		lut.chroma_fine_threshold_rb[i] = fine_threshold * chroma_scale_rb;
		lut.chroma_fine_threshold_g[i] = fine_threshold * chroma_scale_g;
		lut.chroma_mid_threshold[i] = mid_threshold * chroma_mid_scale;
		lut.chroma_fine_keep[i] = std::max(0.0f, fine_keep - (color_noise_value * noise_amount * 0.55f));
		lut.chroma_mid_keep[i] = std::max(0.0f, mid_keep - (color_noise_value * noise_amount * 0.25f));
		lut.temporal_mix_base[i] = temporal_scale * response;
	}
	return lut;
}

struct DenoiseScratch {
	int capacity = 0;
	std::unique_ptr<float[]> fine_r;
	std::unique_ptr<float[]> fine_g;
	std::unique_ptr<float[]> fine_b;
	std::unique_ptr<float[]> base_r;
	std::unique_ptr<float[]> base_g;
	std::unique_ptr<float[]> base_b;
	std::unique_ptr<float[]> tmp_r;
	std::unique_ptr<float[]> tmp_g;
	std::unique_ptr<float[]> tmp_b;

	void ensure(int pixel_count) {
		if (pixel_count <= capacity)
			return;
		capacity = pixel_count;
		fine_r.reset(new float[capacity]);
		fine_g.reset(new float[capacity]);
		fine_b.reset(new float[capacity]);
		base_r.reset(new float[capacity]);
		base_g.reset(new float[capacity]);
		base_b.reset(new float[capacity]);
		tmp_r.reset(new float[capacity]);
		tmp_g.reset(new float[capacity]);
		tmp_b.reset(new float[capacity]);
	}
};

static inline void read_unpremultiplied_rgb(const uchar* px, float& r, float& g, float& b) {
	if (px[3] == 255) {
		r = static_cast<float>(px[0]);
		g = static_cast<float>(px[1]);
		b = static_cast<float>(px[2]);
	} else {
		const float alpha = static_cast<float>(px[3]);
		const float unpremultiply = alpha > 0.0f ? 255.0f / alpha : 0.0f;
		r = static_cast<float>(px[0]) * unpremultiply;
		g = static_cast<float>(px[1]) * unpremultiply;
		b = static_cast<float>(px[2]) * unpremultiply;
	}
}

static void box_blur_image_rgb(const uchar* input_pixels, int input_stride,
                               float* dst_r, float* dst_g, float* dst_b,
                               float* tmp_r, float* tmp_g, float* tmp_b,
                               int width, int height, int radius) {
	const int window = (radius * 2) + 1;
	const float inv_window = 1.0f / static_cast<float>(window);

	#pragma omp parallel for if(width * height >= 16384) schedule(static)
	for (int y = 0; y < height; ++y) {
		const int row = y * width;
		const uchar* row_pixels = input_pixels + (y * input_stride);
		float first_r, first_g, first_b;
		read_unpremultiplied_rgb(row_pixels, first_r, first_g, first_b);
		float sum_r = first_r * static_cast<float>(radius + 1);
		float sum_g = first_g * static_cast<float>(radius + 1);
		float sum_b = first_b * static_cast<float>(radius + 1);
		for (int i = 1; i <= radius; ++i) {
			float sample_r, sample_g, sample_b;
			read_unpremultiplied_rgb(row_pixels + (std::min(i, width - 1) * 4), sample_r, sample_g, sample_b);
			sum_r += sample_r;
			sum_g += sample_g;
			sum_b += sample_b;
		}

		for (int x = 0; x < width; ++x) {
			const int idx = row + x;
			tmp_r[idx] = sum_r * inv_window;
			tmp_g[idx] = sum_g * inv_window;
			tmp_b[idx] = sum_b * inv_window;

			float add_r, add_g, add_b;
			float sub_r, sub_g, sub_b;
			read_unpremultiplied_rgb(row_pixels + (std::min(x + radius + 1, width - 1) * 4), add_r, add_g, add_b);
			read_unpremultiplied_rgb(row_pixels + (std::max(x - radius, 0) * 4), sub_r, sub_g, sub_b);
			sum_r += add_r - sub_r;
			sum_g += add_g - sub_g;
			sum_b += add_b - sub_b;
		}
	}

	#pragma omp parallel for if(width * height >= 16384) schedule(static)
	for (int x = 0; x < width; ++x) {
		float sum_r = tmp_r[x] * static_cast<float>(radius + 1);
		float sum_g = tmp_g[x] * static_cast<float>(radius + 1);
		float sum_b = tmp_b[x] * static_cast<float>(radius + 1);
		for (int i = 1; i <= radius; ++i) {
			const int idx = std::min(i, height - 1) * width + x;
			sum_r += tmp_r[idx];
			sum_g += tmp_g[idx];
			sum_b += tmp_b[idx];
		}

		for (int y = 0; y < height; ++y) {
			const int idx = y * width + x;
			dst_r[idx] = sum_r * inv_window;
			dst_g[idx] = sum_g * inv_window;
			dst_b[idx] = sum_b * inv_window;
			const int add_idx = std::min(y + radius + 1, height - 1) * width + x;
			const int sub_idx = std::max(y - radius, 0) * width + x;
			sum_r += tmp_r[add_idx] - tmp_r[sub_idx];
			sum_g += tmp_g[add_idx] - tmp_g[sub_idx];
			sum_b += tmp_b[add_idx] - tmp_b[sub_idx];
		}
	}
}

static void box_blur_rgb(const float* src_r, const float* src_g, const float* src_b,
                         float* dst_r, float* dst_g, float* dst_b,
                         float* tmp_r, float* tmp_g, float* tmp_b,
                         int width, int height, int radius) {
	const int pixel_count = width * height;
	if (radius <= 0) {
		std::copy(src_r, src_r + pixel_count, dst_r);
		std::copy(src_g, src_g + pixel_count, dst_g);
		std::copy(src_b, src_b + pixel_count, dst_b);
		return;
	}

	const int window = (radius * 2) + 1;
	const float inv_window = 1.0f / static_cast<float>(window);

	#pragma omp parallel for if(width * height >= 16384) schedule(static)
	for (int y = 0; y < height; ++y) {
		const int row = y * width;
		float sum_r = src_r[row] * static_cast<float>(radius + 1);
		float sum_g = src_g[row] * static_cast<float>(radius + 1);
		float sum_b = src_b[row] * static_cast<float>(radius + 1);
		for (int i = 1; i <= radius; ++i) {
			const int idx = row + std::min(i, width - 1);
			sum_r += src_r[idx];
			sum_g += src_g[idx];
			sum_b += src_b[idx];
		}

		for (int x = 0; x < width; ++x) {
			const int idx = row + x;
			tmp_r[idx] = sum_r * inv_window;
			tmp_g[idx] = sum_g * inv_window;
			tmp_b[idx] = sum_b * inv_window;
			const int add_idx = row + std::min(x + radius + 1, width - 1);
			const int sub_idx = row + std::max(x - radius, 0);
			sum_r += src_r[add_idx] - src_r[sub_idx];
			sum_g += src_g[add_idx] - src_g[sub_idx];
			sum_b += src_b[add_idx] - src_b[sub_idx];
		}
	}

	#pragma omp parallel for if(width * height >= 16384) schedule(static)
	for (int x = 0; x < width; ++x) {
		float sum_r = tmp_r[x] * static_cast<float>(radius + 1);
		float sum_g = tmp_g[x] * static_cast<float>(radius + 1);
		float sum_b = tmp_b[x] * static_cast<float>(radius + 1);
		for (int i = 1; i <= radius; ++i) {
			const int idx = std::min(i, height - 1) * width + x;
			sum_r += tmp_r[idx];
			sum_g += tmp_g[idx];
			sum_b += tmp_b[idx];
		}

		for (int y = 0; y < height; ++y) {
			const int idx = y * width + x;
			dst_r[idx] = sum_r * inv_window;
			dst_g[idx] = sum_g * inv_window;
			dst_b[idx] = sum_b * inv_window;
			const int add_idx = std::min(y + radius + 1, height - 1) * width + x;
			const int sub_idx = std::max(y - radius, 0) * width + x;
			sum_r += tmp_r[add_idx] - tmp_r[sub_idx];
			sum_g += tmp_g[add_idx] - tmp_g[sub_idx];
			sum_b += tmp_b[add_idx] - tmp_b[sub_idx];
		}
	}
}

static inline float shrink_detail(float value, float threshold, float keep) {
	const float magnitude = std::abs(value);
	if (magnitude <= threshold)
		return value * keep;
	const float sign = value < 0.0f ? -1.0f : 1.0f;
	return sign * (threshold * keep + (magnitude - threshold));
}
}

DenoiseImage::DenoiseImage()
	: strength(0.78),
	  detail(0.50),
	  temporal(0.35),
	  motion_safety(0.70),
	  color_noise(0.70),
	  last_frame_(-1)
{
	response_curve.Nodes().clear();
	response_curve.Nodes().emplace_back(0, 0.0, 1.0, LINEAR);
	response_curve.Nodes().emplace_back(1, 0.5, 0.55, LINEAR);
	response_curve.Nodes().emplace_back(2, 1.0, 0.20, LINEAR);
	init_effect_details();
}

DenoiseImage::DenoiseImage(Keyframe new_strength, Keyframe new_detail, Keyframe new_temporal,
                           Keyframe new_motion_safety, Keyframe new_color_noise)
	: DenoiseImage()
{
	strength = new_strength;
	detail = new_detail;
	temporal = new_temporal;
	motion_safety = new_motion_safety;
	color_noise = new_color_noise;
}

void DenoiseImage::init_effect_details()
{
	InitEffectInfo();
	info.class_name = "DenoiseImage";
	info.name = "Denoise Image";
	info.description = "Reduces visible grain and color speckles in video frames.";
	info.has_audio = false;
	info.has_video = true;
}

void DenoiseImage::reset_temporal_history()
{
	previous_input_ = QImage();
	last_frame_ = -1;
}

std::shared_ptr<openshot::Frame> DenoiseImage::GetFrame(std::shared_ptr<openshot::Frame> frame, int64_t frame_number)
{
	std::shared_ptr<QImage> frame_image = frame->GetImage();
	if (!frame_image || frame_image->isNull()) {
		reset_temporal_history();
		return frame;
	}

	const float strength_value = clamp01(static_cast<float>(strength.GetValue(frame_number)));
	if (strength_value <= 0.0f) {
		reset_temporal_history();
		return frame;
	}

	if (frame_image->format() != QImage::Format_RGBA8888_Premultiplied)
		*frame_image = frame_image->convertToFormat(QImage::Format_RGBA8888_Premultiplied);

	const int width = frame_image->width();
	const int height = frame_image->height();
	if (width <= 0 || height <= 0) {
		reset_temporal_history();
		return frame;
	}

	const float detail_value = clamp01(static_cast<float>(detail.GetValue(frame_number)));
	const float temporal_value = clamp01(static_cast<float>(temporal.GetValue(frame_number)));
	const float motion_safety_value = clamp01(static_cast<float>(motion_safety.GetValue(frame_number)));
	const float color_noise_value = clamp01(static_cast<float>(color_noise.GetValue(frame_number)));

	QImage input_copy;
	const QImage* input_image = frame_image.get();
	if (temporal_value > 0.0f) {
		input_copy = frame_image->copy();
		input_image = &input_copy;
	} else {
		reset_temporal_history();
	}
	const bool temporal_valid =
		temporal_value > 0.0f &&
		last_frame_ >= 0 &&
		frame_number == last_frame_ + 1 &&
		!previous_input_.isNull() &&
		previous_input_.width() == width &&
		previous_input_.height() == height &&
		previous_input_.format() == QImage::Format_RGBA8888_Premultiplied;

	const std::array<float, 256> response_lut = build_response_lut(response_curve, frame_number);
	QImage output(width, height, QImage::Format_RGBA8888_Premultiplied);

	const uchar* prev_pixels = temporal_valid ? previous_input_.constBits() : nullptr;
	const uchar* input_pixels = input_image->constBits();
	uchar* output_pixels = output.bits();
	const int input_stride = input_image->bytesPerLine();
	const int output_stride = output.bytesPerLine();
	const int prev_stride = temporal_valid ? previous_input_.bytesPerLine() : 0;
	const int pixel_count = width * height;
	const DenoisePixelLut pixel_lut = build_pixel_lut(response_lut, strength_value, detail_value, temporal_value, color_noise_value);
	const float inverse_detail = 1.0f - detail_value;
	const float temporal_gate_scale = 0.45f + (0.55f * inverse_detail);
	const float temporal_luma_scale = 0.32f + (inverse_detail * 0.28f);
	const float temporal_chroma_scale = 0.65f + (color_noise_value * 0.35f);
	const float motion_low = 0.020f + ((1.0f - motion_safety_value) * 0.080f);
	const float motion_high = motion_low + 0.055f + ((1.0f - motion_safety_value) * 0.100f);

	// Per-thread scratch avoids repeated full-frame allocations without sharing buffers across render threads.
	thread_local DenoiseScratch scratch;
	scratch.ensure(pixel_count);
	float* fine_r = scratch.fine_r.get();
	float* fine_g = scratch.fine_g.get();
	float* fine_b = scratch.fine_b.get();
	float* base_r = scratch.base_r.get();
	float* base_g = scratch.base_g.get();
	float* base_b = scratch.base_b.get();
	float* tmp_r = scratch.tmp_r.get();
	float* tmp_g = scratch.tmp_g.get();
	float* tmp_b = scratch.tmp_b.get();

	const int fine_radius = 1 + (strength_value >= 0.75f && detail_value <= 0.45f ? 1 : 0);
	int coarse_radius = 3;
	if (strength_value >= 0.95f && detail_value <= 0.15f)
		coarse_radius = 8;
	else if (strength_value >= 0.80f && detail_value <= 0.35f)
		coarse_radius = 6;
	else if (strength_value >= 0.65f && detail_value <= 0.55f)
		coarse_radius = 5;

	box_blur_image_rgb(input_pixels, input_stride, fine_r, fine_g, fine_b, tmp_r, tmp_g, tmp_b, width, height, fine_radius);
	box_blur_rgb(fine_r, fine_g, fine_b, base_r, base_g, base_b, tmp_r, tmp_g, tmp_b, width, height, coarse_radius);

	#pragma omp parallel for if(width * height >= 16384) schedule(static)
	for (int y = 0; y < height; ++y) {
		for (int x = 0; x < width; ++x) {
			const int p = (y * width) + x;
			const uchar* input_pixel = input_pixels + (y * input_stride) + (x * 4);
			float center_r, center_g, center_b;
			read_unpremultiplied_rgb(input_pixel, center_r, center_g, center_b);
			const float center_y = luma(center_r, center_g, center_b);
			const float fine_y_value = luma(fine_r[p], fine_g[p], fine_b[p]);
			const float base_y_value = luma(base_r[p], base_g[p], base_b[p]);
			const float fine_detail_y = center_y - fine_y_value;
			const float mid_detail_y = fine_y_value - base_y_value;

			const int response_index = clampByte(center_y);
			const float fine_threshold = pixel_lut.fine_threshold[response_index];
			const float mid_threshold = pixel_lut.mid_threshold[response_index];
			const float fine_keep = pixel_lut.fine_keep[response_index];
			const float mid_keep = pixel_lut.mid_keep[response_index];
			const float chroma_mid_threshold = pixel_lut.chroma_mid_threshold[response_index];
			const float chroma_fine_keep = pixel_lut.chroma_fine_keep[response_index];
			const float chroma_mid_keep = pixel_lut.chroma_mid_keep[response_index];

			float out_y = base_y_value +
				shrink_detail(mid_detail_y, mid_threshold, mid_keep) +
				shrink_detail(fine_detail_y, fine_threshold, fine_keep);

			const float fine_detail_r = center_r - fine_r[p];
			const float mid_detail_r = fine_r[p] - base_r[p];
			const float fine_detail_g = center_g - fine_g[p];
			const float mid_detail_g = fine_g[p] - base_g[p];
			const float fine_detail_b = center_b - fine_b[p];
			const float mid_detail_b = fine_b[p] - base_b[p];
			float out_r = base_r[p] +
				shrink_detail(mid_detail_r, chroma_mid_threshold, chroma_mid_keep) +
				shrink_detail(fine_detail_r, pixel_lut.chroma_fine_threshold_rb[response_index], chroma_fine_keep);
			float out_g = base_g[p] +
				shrink_detail(mid_detail_g, chroma_mid_threshold, chroma_mid_keep) +
				shrink_detail(fine_detail_g, pixel_lut.chroma_fine_threshold_g[response_index], chroma_fine_keep);
			float out_b = base_b[p] +
				shrink_detail(mid_detail_b, chroma_mid_threshold, chroma_mid_keep) +
				shrink_detail(fine_detail_b, pixel_lut.chroma_fine_threshold_rb[response_index], chroma_fine_keep);

			const float reconstructed_y = luma(out_r, out_g, out_b);
			out_r += out_y - reconstructed_y;
			out_g += out_y - reconstructed_y;
			out_b += out_y - reconstructed_y;

			if (temporal_valid) {
				const uchar* prev_pixel = prev_pixels + (y * prev_stride) + (x * 4);
				float prev_r;
				float prev_g;
				float prev_b;
				if (prev_pixel[3] == 255) {
					prev_r = static_cast<float>(prev_pixel[0]);
					prev_g = static_cast<float>(prev_pixel[1]);
					prev_b = static_cast<float>(prev_pixel[2]);
				} else {
					const float prev_alpha = static_cast<float>(prev_pixel[3]);
					const float prev_unpremultiply = prev_alpha > 0.0f ? 255.0f / prev_alpha : 0.0f;
					prev_r = static_cast<float>(prev_pixel[0]) * prev_unpremultiply;
					prev_g = static_cast<float>(prev_pixel[1]) * prev_unpremultiply;
					prev_b = static_cast<float>(prev_pixel[2]) * prev_unpremultiply;
				}
				const float prev_y = luma(prev_r, prev_g, prev_b);

				const float luma_motion = std::abs(center_y - prev_y) * kInv255;
				const float color_motion = (
					std::abs(center_r - prev_r) +
					std::abs(center_g - prev_g) +
					std::abs(center_b - prev_b)) * (kInv255 / 3.0f);
				const float motion = std::max(luma_motion, color_motion * 0.75f);
				const float motion_gate = 1.0f - smoothstep(motion_low, motion_high, motion);
				const float temporal_mix = std::min(0.35f, pixel_lut.temporal_mix_base[response_index] * motion_gate * temporal_gate_scale);

				const float temporal_luma_mix = temporal_mix * temporal_luma_scale;
				const float temporal_chroma_mix = temporal_mix * temporal_chroma_scale;

				const float out_chroma_r = out_r - out_y;
				const float out_chroma_g = out_g - out_y;
				const float out_chroma_b = out_b - out_y;
				out_y = lerp(out_y, prev_y, temporal_luma_mix);
				out_r = out_y + lerp(out_chroma_r, prev_r - prev_y, temporal_chroma_mix);
				out_g = out_y + lerp(out_chroma_g, prev_g - prev_y, temporal_chroma_mix);
				out_b = out_y + lerp(out_chroma_b, prev_b - prev_y, temporal_chroma_mix);
			}

			uchar* output_pixel = output_pixels + (y * output_stride) + (x * 4);
			if (input_pixel[3] == 255) {
				output_pixel[0] = static_cast<uchar>(clampByte(out_r));
				output_pixel[1] = static_cast<uchar>(clampByte(out_g));
				output_pixel[2] = static_cast<uchar>(clampByte(out_b));
			} else {
				const float alpha = static_cast<float>(input_pixel[3]);
				const float premultiply = alpha * kInv255;
				output_pixel[0] = static_cast<uchar>(clampByte(out_r * premultiply));
				output_pixel[1] = static_cast<uchar>(clampByte(out_g * premultiply));
				output_pixel[2] = static_cast<uchar>(clampByte(out_b * premultiply));
			}
			output_pixel[3] = input_pixel[3];
		}
	}

	*frame_image = output;
	if (temporal_value > 0.0f)
		previous_input_ = input_copy;
	last_frame_ = frame_number;
	return frame;
}

std::string DenoiseImage::Json() const
{
	return JsonValue().toStyledString();
}

Json::Value DenoiseImage::JsonValue() const
{
	Json::Value root = EffectBase::JsonValue();
	root["type"] = info.class_name;
	root["strength"] = strength.JsonValue();
	root["detail"] = detail.JsonValue();
	root["temporal"] = temporal.JsonValue();
	root["motion_safety"] = motion_safety.JsonValue();
	root["color_noise"] = color_noise.JsonValue();
	root["response_curve"] = response_curve.JsonValue();
	return root;
}

void DenoiseImage::SetJson(const std::string value)
{
	try {
		SetJsonValue(openshot::stringToJson(value));
	} catch (const std::exception&) {
		throw InvalidJSON("JSON is invalid (missing keys or invalid data types)");
	}
}

void DenoiseImage::SetJsonValue(const Json::Value root)
{
	EffectBase::SetJsonValue(root);
	if (!root["strength"].isNull())
		strength.SetJsonValue(root["strength"]);
	if (!root["detail"].isNull())
		detail.SetJsonValue(root["detail"]);
	if (!root["temporal"].isNull())
		temporal.SetJsonValue(root["temporal"]);
	if (!root["motion_safety"].isNull())
		motion_safety.SetJsonValue(root["motion_safety"]);
	if (!root["color_noise"].isNull())
		color_noise.SetJsonValue(root["color_noise"]);
	if (!root["response_curve"].isNull())
		response_curve.SetJsonValue(root["response_curve"]);
	reset_temporal_history();
}

std::string DenoiseImage::PropertiesJSON(int64_t requested_frame) const
{
	Json::Value root = BasePropertiesJSON(requested_frame);
	root["strength"] = add_property_json("Strength", strength.GetValue(requested_frame), "float", "", &strength, 0.0, 1.0, false, requested_frame);
	root["detail"] = add_property_json("Detail", detail.GetValue(requested_frame), "float", "", &detail, 0.0, 1.0, false, requested_frame);
	root["temporal"] = add_property_json("Temporal", temporal.GetValue(requested_frame), "float", "", &temporal, 0.0, 1.0, false, requested_frame);
	root["motion_safety"] = add_property_json("Motion Safety", motion_safety.GetValue(requested_frame), "float", "", &motion_safety, 0.0, 1.0, false, requested_frame);
	root["color_noise"] = add_property_json("Color Noise", color_noise.GetValue(requested_frame), "float", "", &color_noise, 0.0, 1.0, false, requested_frame);
	root["response_curve"] = add_property_json("Response Curve", 0.0, "colorgrade_curve", response_curve.Summary(requested_frame), NULL, 0.0, 1.0, false, requested_frame);
	root["response_curve"]["curve"] = response_curve.JsonValue();
	root["response_curve"]["channel"] = "response";
	root["response_curve"]["summary"] = response_curve.Summary(requested_frame);
	return root.toStyledString();
}
