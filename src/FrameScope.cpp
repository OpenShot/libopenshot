/**
 * @file
 * @brief Source file for FrameScope class
 * @author Jonathan Thomas <jonathan@openshot.org>
 *
 * @ref License
 */

// Copyright (c) 2008-2026 OpenShot Studios, LLC
//
// SPDX-License-Identifier: LGPL-3.0-or-later

#include "FrameScope.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <limits>

using namespace openshot;

namespace {
constexpr float kInv255 = 1.0f / 255.0f;
constexpr float kVectorscopeUMax = 0.43600f;
constexpr float kVectorscopeVMax = 0.61500f;

static int clamp_int(int value, int min_value, int max_value) {
	return std::max(min_value, std::min(max_value, value));
}

static const std::array<float, 256>& inv_alpha_lut() {
	static const std::array<float, 256> lut = [] {
		std::array<float, 256> values{};
		values[0] = 0.0f;
		for (int i = 1; i < 256; ++i)
			values[i] = 255.0f / static_cast<float>(i);
		return values;
	}();
	return lut;
}

static Json::Value json_array_from_vector(const std::vector<int>& values) {
	Json::Value array(Json::arrayValue);
	for (size_t i = 0; i < values.size(); ++i)
		array.append(values[i]);
	return array;
}

static Json::Value json_array_from_vector(const std::vector<uint32_t>& values) {
	Json::Value array(Json::arrayValue);
	for (size_t i = 0; i < values.size(); ++i)
		array.append(Json::Value::UInt(values[i]));
	return array;
}

static Json::Value json_array_from_vector(const std::vector<float>& values) {
	Json::Value array(Json::arrayValue);
	for (size_t i = 0; i < values.size(); ++i)
		array.append(values[i]);
	return array;
}
}

FrameScope::FrameScope()
	: frame(nullptr),
	  waveform_columns(256),
	  audio_buckets(256),
	  vectorscope_size(256),
	  roi_enabled(false),
	  roi_x(0.0f),
	  roi_y(0.0f),
	  roi_width(1.0f),
	  roi_height(1.0f),
	  waveform_bins(256),
	  waveform_column_map_width(0),
	  waveform_column_map_columns(0),
	  json_dirty(true) {
	reset();
}

FrameScope::FrameScope(std::shared_ptr<Frame> new_frame, int new_waveform_columns, int new_audio_buckets, int new_vectorscope_size)
	: frame(new_frame),
	  waveform_columns(std::max(1, new_waveform_columns)),
	  audio_buckets(std::max(1, new_audio_buckets)),
	  vectorscope_size(std::max(1, new_vectorscope_size)),
	  roi_enabled(false),
	  roi_x(0.0f),
	  roi_y(0.0f),
	  roi_width(1.0f),
	  roi_height(1.0f),
	  waveform_bins(256),
	  waveform_column_map_width(0),
	  waveform_column_map_columns(0),
	  json_dirty(true) {
	analyze();
}

void FrameScope::reset() {
	reset_video();
	reset_audio();
	json_dirty = true;
}

void FrameScope::reset_video() {
	video_present = false;
	video_width = 0;
	video_height = 0;
	avg_luma = 0.0;
	clipped_shadows = 0;
	clipped_highlights = 0;
	clipped_red = 0;
	clipped_green = 0;
	clipped_blue = 0;
	ensure_video_buffers();
	std::fill(histogram_luma.begin(), histogram_luma.end(), 0u);
	std::fill(histogram_red.begin(), histogram_red.end(), 0u);
	std::fill(histogram_green.begin(), histogram_green.end(), 0u);
	std::fill(histogram_blue.begin(), histogram_blue.end(), 0u);
	std::fill(waveform_luma.begin(), waveform_luma.end(), 0u);
	std::fill(waveform_red.begin(), waveform_red.end(), 0u);
	std::fill(waveform_green.begin(), waveform_green.end(), 0u);
	std::fill(waveform_blue.begin(), waveform_blue.end(), 0u);
	std::fill(vectorscope.begin(), vectorscope.end(), 0u);
	json_dirty = true;
}

void FrameScope::reset_audio() {
	audio_present = false;
	audio_channels = 0;
	audio_samples = 0;
	audio_sample_rate = 0;
	audio_peak.clear();
	audio_rms.clear();
	audio_clipped_samples.clear();
	audio_waveform_min.clear();
	audio_waveform_max.clear();
	json_dirty = true;
}

void FrameScope::ensure_video_buffers() {
	histogram_luma.resize(256);
	histogram_red.resize(256);
	histogram_green.resize(256);
	histogram_blue.resize(256);
	waveform_luma.resize(static_cast<size_t>(waveform_columns) * static_cast<size_t>(waveform_bins));
	waveform_red.resize(static_cast<size_t>(waveform_columns) * static_cast<size_t>(waveform_bins));
	waveform_green.resize(static_cast<size_t>(waveform_columns) * static_cast<size_t>(waveform_bins));
	waveform_blue.resize(static_cast<size_t>(waveform_columns) * static_cast<size_t>(waveform_bins));
	vectorscope.resize(static_cast<size_t>(vectorscope_size) * static_cast<size_t>(vectorscope_size));
}

void FrameScope::ensure_audio_buffers() {
	audio_peak.assign(static_cast<size_t>(audio_channels), 0.0f);
	audio_rms.assign(static_cast<size_t>(audio_channels), 0.0f);
	audio_clipped_samples.assign(static_cast<size_t>(audio_channels), 0u);
	audio_waveform_min.assign(static_cast<size_t>(audio_channels), std::vector<float>(static_cast<size_t>(audio_buckets), 0.0f));
	audio_waveform_max.assign(static_cast<size_t>(audio_channels), std::vector<float>(static_cast<size_t>(audio_buckets), 0.0f));
}

void FrameScope::rebuild_waveform_column_map(int width) {
	if (width == waveform_column_map_width && waveform_columns == waveform_column_map_columns &&
		static_cast<int>(waveform_column_map.size()) == width)
		return;

	waveform_column_map.resize(static_cast<size_t>(width));
	waveform_offset_map.resize(static_cast<size_t>(width));
	const int waveform_column_limit = waveform_columns - 1;
	for (int x = 0; x < width; ++x) {
		const int col = clamp_int((x * waveform_columns) / std::max(1, width), 0, waveform_column_limit);
		waveform_column_map[static_cast<size_t>(x)] = col;
		waveform_offset_map[static_cast<size_t>(x)] = static_cast<size_t>(col) * static_cast<size_t>(waveform_bins);
	}

	waveform_column_map_width = width;
	waveform_column_map_columns = waveform_columns;
}

void FrameScope::SetFrame(std::shared_ptr<Frame> new_frame) {
	frame = new_frame;
	analyze();
}

void FrameScope::SetWaveformColumns(int columns) {
	waveform_columns = std::max(1, columns);
	reset_video();
	if (frame)
		analyze_video();
}

void FrameScope::SetAudioBuckets(int buckets) {
	audio_buckets = std::max(1, buckets);
	reset_audio();
	if (frame)
		analyze_audio();
}

void FrameScope::SetVectorscopeSize(int size) {
	vectorscope_size = std::max(1, size);
	reset_video();
	if (frame)
		analyze_video();
}

void FrameScope::SetVideoRegionNormalized(float x, float y, float width, float height) {
	roi_x = std::max(0.0f, std::min(1.0f, x));
	roi_y = std::max(0.0f, std::min(1.0f, y));
	roi_width = std::max(0.0f, std::min(1.0f - roi_x, width));
	roi_height = std::max(0.0f, std::min(1.0f - roi_y, height));
	roi_enabled = roi_width > 0.0f && roi_height > 0.0f;
	reset_video();
	if (frame)
		analyze_video();
}

void FrameScope::ClearVideoRegion() {
	roi_enabled = false;
	roi_x = 0.0f;
	roi_y = 0.0f;
	roi_width = 1.0f;
	roi_height = 1.0f;
	reset_video();
	if (frame)
		analyze_video();
}

void FrameScope::analyze() {
	reset();
	if (!frame)
		return;

	analyze_video();
	analyze_audio();
	json_dirty = true;
}

void FrameScope::analyze_video() {
	// Frame images are always QImage::Format_RGBA8888_Premultiplied (enforced
	// by Frame::AddImage). Pixel byte order is [R=0, G=1, B=2, A=3].
	std::shared_ptr<QImage> image = frame->GetImage();
	if (!image || image->isNull())
		return;

	video_present = true;
	const int width = image->width();
	const int height = image->height();
	int start_x = 0;
	int end_x = width;
	int start_y = 0;
	int end_y = height;
	if (roi_enabled) {
		start_x = clamp_int(static_cast<int>(std::floor(roi_x * width)), 0, width - 1);
		start_y = clamp_int(static_cast<int>(std::floor(roi_y * height)), 0, height - 1);
		end_x = clamp_int(static_cast<int>(std::ceil((roi_x + roi_width) * width)), start_x + 1, width);
		end_y = clamp_int(static_cast<int>(std::ceil((roi_y + roi_height) * height)), start_y + 1, height);
	}
	video_width = std::max(1, end_x - start_x);
	video_height = std::max(1, end_y - start_y);
	ensure_video_buffers();

	double luma_sum = 0.0;
	int64_t pixel_count = 0;
	clipped_shadows = 0;
	clipped_highlights = 0;
	clipped_red = 0;
	clipped_green = 0;
	clipped_blue = 0;

	const int bytes_per_line = image->bytesPerLine();
	const unsigned char* bits = image->constBits();
	const auto& inv_alpha = inv_alpha_lut();
	rebuild_waveform_column_map(video_width);
	const float vectorscope_center = static_cast<float>(vectorscope_size - 1) * 0.5f;
	const float vectorscope_scale = vectorscope_center;

	for (int y = start_y; y < end_y; ++y) {
		// Use pointer increment instead of per-pixel index arithmetic (x * 4).
		const unsigned char* pixel = bits + (static_cast<size_t>(y) * bytes_per_line)
		                                   + (static_cast<size_t>(start_x) * 4);
		int roi_column = 0;
		for (int x = start_x; x < end_x; ++x, ++roi_column, pixel += 4) {
			const int red   = pixel[0];  // RGBA8888: [R=0, G=1, B=2, A=3]
			const int green = pixel[1];
			const int blue  = pixel[2];
			const int alpha = pixel[3];  // premultiplied — divided out below
			if (alpha <= 0)
				continue;

			int red_idx, green_idx, blue_idx;
			float redf, greenf, bluef;
			if (alpha == 255) {
				redf   = red   * kInv255;
				greenf = green * kInv255;
				bluef  = blue  * kInv255;
				// For fully-opaque pixels the bin index is just the raw byte
				// value — no float rounding needed (round(byte/255 * 255) == byte).
				red_idx   = red;
				green_idx = green;
				blue_idx  = blue;
			} else {
				const float inv_a = inv_alpha[alpha];
				redf   = std::min(1.0f, (red   * inv_a) * kInv255);
				greenf = std::min(1.0f, (green * inv_a) * kInv255);
				bluef  = std::min(1.0f, (blue  * inv_a) * kInv255);
				// All values clamped to [0,1], so val*255+0.5 ∈ [0,255.5] — cast is safe.
				red_idx   = static_cast<int>(redf   * 255.0f + 0.5f);
				green_idx = static_cast<int>(greenf * 255.0f + 0.5f);
				blue_idx  = static_cast<int>(bluef  * 255.0f + 0.5f);
			}
			const float luma = 0.299f * redf + 0.587f * greenf + 0.114f * bluef;
			// luma ∈ [0,1] (weighted sum of [0,1] values), so luma*255+0.5 ∈ [0,255.5].
			const int luma_idx = static_cast<int>(luma * 255.0f + 0.5f);

			// Pre-multiplied offset: eliminates a runtime multiply per pixel.
			const size_t waveform_offset = waveform_offset_map[static_cast<size_t>(roi_column)];

			const float u = -0.14713f * redf - 0.28886f * greenf + 0.43600f * bluef;
			const float v =  0.61500f * redf - 0.51499f * greenf - 0.10001f * bluef;
			const float normalized_u = u / kVectorscopeUMax;
			const float normalized_v = v / kVectorscopeVMax;
			// vectorscope_center + normalized_{u,v} * vectorscope_scale is always in
			// [0, vectorscope_size-1]; adding 0.5 before truncation equals std::round
			// for all non-negative values.
			const int vector_x = clamp_int(static_cast<int>(vectorscope_center + (normalized_u * vectorscope_scale) + 0.5f), 0, vectorscope_size - 1);
			const int vector_y = clamp_int(static_cast<int>(vectorscope_center - (normalized_v * vectorscope_scale) + 0.5f), 0, vectorscope_size - 1);
			const size_t vector_offset = (static_cast<size_t>(vector_y) * static_cast<size_t>(vectorscope_size)) + static_cast<size_t>(vector_x);

			histogram_luma[luma_idx]++;
			histogram_red[red_idx]++;
			histogram_green[green_idx]++;
			histogram_blue[blue_idx]++;
			waveform_luma[waveform_offset + luma_idx]++;
			waveform_red[waveform_offset + red_idx]++;
			waveform_green[waveform_offset + green_idx]++;
			waveform_blue[waveform_offset + blue_idx]++;
			vectorscope[vector_offset]++;

			luma_sum += luma;
			++pixel_count;
			if (luma_idx <= 2)   ++clipped_shadows;
			if (luma_idx >= 253) ++clipped_highlights;
			if (red_idx   >= 253) ++clipped_red;
			if (green_idx >= 253) ++clipped_green;
			if (blue_idx  >= 253) ++clipped_blue;
		}
	}

	avg_luma = pixel_count > 0 ? (luma_sum / static_cast<double>(pixel_count)) : 0.0;
}

void FrameScope::analyze_audio() {
	if (!frame->has_audio_data || !frame->audio)
		return;

	const int channels = frame->GetAudioChannelsCount();
	const int samples = frame->GetAudioSamplesCount();
	if (channels <= 0 || samples <= 0)
		return;

	audio_present = true;
	audio_channels = channels;
	audio_samples = samples;
	audio_sample_rate = frame->SampleRate();
	ensure_audio_buffers();
	std::vector<double> rms_sums(static_cast<size_t>(channels), 0.0);

	for (int channel = 0; channel < channels; ++channel) {
		float* channel_samples = frame->GetAudioSamples(channel);
		if (!channel_samples)
			continue;

		std::fill(audio_waveform_min[channel].begin(), audio_waveform_min[channel].end(), 1.0f);
		std::fill(audio_waveform_max[channel].begin(), audio_waveform_max[channel].end(), -1.0f);

		for (int sample = 0; sample < samples; ++sample) {
			const float value = channel_samples[sample];
			const float abs_value = std::abs(value);
			const int bucket = clamp_int((sample * audio_buckets) / std::max(1, samples), 0, audio_buckets - 1);

			audio_peak[channel] = std::max(audio_peak[channel], abs_value);
			rms_sums[channel] += static_cast<double>(value) * static_cast<double>(value);
			if (abs_value >= 0.999f)
				audio_clipped_samples[channel]++;

			audio_waveform_min[channel][bucket] = std::min(audio_waveform_min[channel][bucket], value);
			audio_waveform_max[channel][bucket] = std::max(audio_waveform_max[channel][bucket], value);
		}

		for (int bucket = 0; bucket < audio_buckets; ++bucket) {
			if (audio_waveform_min[channel][bucket] > audio_waveform_max[channel][bucket]) {
				audio_waveform_min[channel][bucket] = 0.0f;
				audio_waveform_max[channel][bucket] = 0.0f;
			}
		}
	}

	for (int channel = 0; channel < channels; ++channel) {
		audio_rms[channel] = samples > 0 ? static_cast<float>(std::sqrt(rms_sums[channel] / static_cast<double>(samples))) : 0.0f;
	}
}

void FrameScope::rebuild_json() const {
	scope_data = Json::Value(Json::objectValue);
	scope_data["version"] = 1;

	Json::Value video(Json::objectValue);
	video["present"] = video_present;
	if (video_present) {
		video["width"] = video_width;
		video["height"] = video_height;

		video["summary"] = Json::Value(Json::objectValue);
		video["summary"]["avg_luma"] = avg_luma;
		video["summary"]["clipped_shadows"] = clipped_shadows;
		video["summary"]["clipped_highlights"] = clipped_highlights;
		video["summary"]["clipped_red"] = clipped_red;
		video["summary"]["clipped_green"] = clipped_green;
		video["summary"]["clipped_blue"] = clipped_blue;

		video["histogram"] = Json::Value(Json::objectValue);
		video["histogram"]["luma"] = json_array_from_vector(histogram_luma);
		video["histogram"]["red"] = json_array_from_vector(histogram_red);
		video["histogram"]["green"] = json_array_from_vector(histogram_green);
		video["histogram"]["blue"] = json_array_from_vector(histogram_blue);

		video["waveform"] = Json::Value(Json::objectValue);
		video["waveform"]["columns"] = waveform_columns;
		video["waveform"]["bins"] = waveform_bins;
		video["waveform"]["luma"] = json_array_from_vector(waveform_luma);
		video["waveform"]["red"] = json_array_from_vector(waveform_red);
		video["waveform"]["green"] = json_array_from_vector(waveform_green);
		video["waveform"]["blue"] = json_array_from_vector(waveform_blue);

		video["vectorscope"] = Json::Value(Json::objectValue);
		video["vectorscope"]["size"] = vectorscope_size;
		video["vectorscope"]["density"] = json_array_from_vector(vectorscope);
	}
	scope_data["video"] = video;

	Json::Value audio(Json::objectValue);
	audio["present"] = audio_present;
	if (audio_present) {
		audio["channels"] = audio_channels;
		audio["samples"] = audio_samples;
		audio["sample_rate"] = audio_sample_rate;

		audio["summary"] = Json::Value(Json::objectValue);
		audio["summary"]["peak"] = json_array_from_vector(audio_peak);
		audio["summary"]["rms"] = json_array_from_vector(audio_rms);
		audio["summary"]["clipped_samples"] = json_array_from_vector(audio_clipped_samples);

		audio["waveform"] = Json::Value(Json::objectValue);
		audio["waveform"]["buckets"] = audio_buckets;
		audio["waveform"]["min"] = Json::Value(Json::arrayValue);
		audio["waveform"]["max"] = Json::Value(Json::arrayValue);
		for (int channel = 0; channel < audio_channels; ++channel) {
			audio["waveform"]["min"].append(json_array_from_vector(audio_waveform_min[static_cast<size_t>(channel)]));
			audio["waveform"]["max"].append(json_array_from_vector(audio_waveform_max[static_cast<size_t>(channel)]));
		}
	}
	scope_data["audio"] = audio;
	json_dirty = false;
}

Json::Value FrameScope::JsonValue() const {
	if (json_dirty)
		rebuild_json();
	return scope_data;
}

std::string FrameScope::Json() const {
	if (json_dirty)
		rebuild_json();
	return scope_data.toStyledString();
}

std::vector<int> FrameScope::copy_to_int_vector(const std::vector<uint32_t>& values) {
	std::vector<int> copy(values.size(), 0);
	const uint32_t max_int = static_cast<uint32_t>(std::numeric_limits<int>::max());
	for (size_t i = 0; i < values.size(); ++i)
		copy[i] = static_cast<int>(std::min(values[i], max_int));
	return copy;
}

std::vector<float> FrameScope::GetAudioWaveformMin(int channel) const {
	if (channel < 0 || channel >= static_cast<int>(audio_waveform_min.size()))
		return std::vector<float>();
	return audio_waveform_min[static_cast<size_t>(channel)];
}

std::vector<float> FrameScope::GetAudioWaveformMax(int channel) const {
	if (channel < 0 || channel >= static_cast<int>(audio_waveform_max.size()))
		return std::vector<float>();
	return audio_waveform_max[static_cast<size_t>(channel)];
}
