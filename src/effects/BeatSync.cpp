/**
 * @file
 * @brief Source file for BeatSync effect class
 * @author Jonathan Thomas <jonathan@openshot.org>
 *
 * @ref License
 */

// Copyright (c) 2008-2026 OpenShot Studios, LLC
//
// SPDX-License-Identifier: LGPL-3.0-or-later

#include "BeatSync.h"
#include "Exceptions.h"
#include "Timeline.h"

#include <algorithm>
#include <cmath>
#include <vector>

#include <QImage>

#include <AppConfig.h>
#include <juce_audio_basics/juce_audio_basics.h>

using namespace openshot;

namespace {
	constexpr double PI = 3.14159265358979323846;

	inline float clampf(float v, float lo, float hi) {
		return v < lo ? lo : (v > hi ? hi : v);
	}

	inline int clampi(int v, int lo, int hi) {
		return v < lo ? lo : (v > hi ? hi : v);
	}

	inline int blend_channel(int low, int high, int inv, int blend) {
		return (clampi(low, 0, 255) * inv + clampi(high, 0, 255) * blend) >> 8;
	}

	// Logarithmic normalized [0,1] → Hz mapping (20–20000 Hz), matching AudioVisualization
	float normalized_frequency_to_hz(float value) {
		const float min_hz = 20.0f;
		const float max_hz = 20000.0f;
		const float n = clampf(value, 0.0f, 1.0f);
		return min_hz * std::pow(max_hz / min_hz, n);
	}

	float hz_to_normalized_frequency(float hz) {
		if (hz >= 0.0f && hz <= 1.0f)
			return hz;
		const float min_hz = 20.0f;
		const float max_hz = 20000.0f;
		hz = clampf(hz, min_hz, max_hz);
		return clampf(std::log(hz / min_hz) / std::log(max_hz / min_hz), 0.0f, 1.0f);
	}

	// Time-domain bandpass energy: difference of two first-order IIR low-pass filters.
	// Returns linear 0-1 based on RMS + peak of the filtered signal, scaled by gain.
	// This mirrors the reactive_level() approach in AudioVisualization but adds band filtering.
	float band_energy(const std::shared_ptr<Frame>& frame, float low_hz, float high_hz, float gain) {
		const int samples = frame->GetAudioSamplesCount();
		const int channels = frame->GetAudioChannelsCount();
		const int sample_rate = std::max(1, frame->SampleRate());
		if (samples <= 0 || channels <= 0)
			return 0.0f;

		const float nyquist = sample_rate * 0.5f;
		low_hz  = clampf(low_hz,  0.0f, nyquist - 1.0f);
		high_hz = clampf(high_hz, low_hz + 1.0f, nyquist);

		// First-order IIR coefficients: alpha = dt / (tau + dt), tau = 1/(2*pi*fc)
		const float dt = 1.0f / sample_rate;
		const float alpha_hi = dt / (dt + 1.0f / (2.0f * (float)PI * high_hz));
		const float alpha_lo = low_hz > 1.0f
			? dt / (dt + 1.0f / (2.0f * (float)PI * low_hz))
			: 0.0f;  // no high-pass when low_hz is at/near DC

		auto* buffer = frame->GetAudioSampleBuffer();
		std::vector<const float*> ch(channels);
		for (int c = 0; c < channels; ++c)
			ch[c] = buffer->getReadPointer(c);

		float lp_hi = 0.0f, lp_lo = 0.0f;
		double sum_sq = 0.0;
		float peak = 0.0f;
		for (int s = 0; s < samples; ++s) {
			float x = 0.0f;
			for (int c = 0; c < channels; ++c)
				x += ch[c][s];
			x /= channels;

			lp_hi += alpha_hi * (x - lp_hi);  // low-pass at high_hz
			lp_lo += alpha_lo * (x - lp_lo);  // low-pass at low_hz
			const float filtered = lp_hi - lp_lo;  // bandpass = difference

			const float abs_v = std::fabs(filtered);
			sum_sq += abs_v * abs_v;
			peak = std::max(peak, abs_v);
		}

		const float rms = std::sqrt((float)(sum_sq / samples));
		// Combine RMS and peak with the same weighting reactive_level() uses
		const float combined = std::max(rms * 3.6f, peak * 1.15f);
		return clampf(combined * gain, 0.0f, 1.0f);
	}
}

BeatSync::BeatSync() :
	low_color((unsigned char)0, (unsigned char)0, (unsigned char)0, (unsigned char)255),
	high_color((unsigned char)255, (unsigned char)255, (unsigned char)255, (unsigned char)255),
	intensity(2.0),
	threshold(0.1),
	attack_ms(10.0),
	decay_ms(200.0),
	frequency_low(0.0),
	frequency_high(1.0),
	invert(false),
	envelope_(0.0f),
	last_frame_(-1)
{
	init_effect_details();
}

BeatSync::BeatSync(Color color, Keyframe intensity) :
	BeatSync()
{
	this->high_color = color;
	this->intensity = intensity;
}

void BeatSync::init_effect_details()
{
	InitEffectInfo();
	info.class_name = "BeatSync";
	info.name = "Beat Sync";
	info.description = "Generates an audio-reactive color flash layer, synchronized to the beat.";
	info.has_audio = false;
	info.has_video = true;
}

std::shared_ptr<openshot::Frame> BeatSync::GetFrame(std::shared_ptr<openshot::Frame> frame, int64_t frame_number)
{
	const std::shared_ptr<QImage> frame_image = frame->GetImage();
	int width  = frame_image ? std::max(1, frame_image->width())  : 1;
	int height = frame_image ? std::max(1, frame_image->height()) : 1;
	if ((width <= 1 || height <= 1) && ParentTimeline()) {
		if (Timeline* timeline = dynamic_cast<Timeline*>(ParentTimeline())) {
			if (timeline->info.width > 1 && timeline->info.height > 1) {
				width  = timeline->info.width;
				height = timeline->info.height;
			}
		}
	}

	// Reset envelope on seek (discontinuous frame access)
	if (last_frame_ >= 0 && std::abs(frame_number - last_frame_) >= 2)
		envelope_ = 0.0f;
	last_frame_ = frame_number;

	// --- Audio energy for the configured frequency band ---
	const float low_hz  = normalized_frequency_to_hz(clampf(frequency_low.GetValue(frame_number), 0.0f, 1.0f));
	const float high_hz = std::max(low_hz + 1.0f,
		normalized_frequency_to_hz(clampf(frequency_high.GetValue(frame_number), 0.0f, 1.0f)));
	const float gain_v  = std::max(0.01f, (float)intensity.GetValue(frame_number));
	const float raw_energy = band_energy(frame, low_hz, high_hz, gain_v);

	// --- IIR envelope follower ---
	// Derive per-frame time constant from audio context
	const int samples = frame->GetAudioSamplesCount();
	const int sample_rate = std::max(1, frame->SampleRate());
	const float frame_sec = (samples > 0) ? (float)samples / sample_rate : 1.0f / 24.0f;

	const float atk = clampf((float)attack_ms.GetValue(frame_number), 1.0f, 5000.0f);
	const float dec = clampf((float)decay_ms.GetValue(frame_number), 1.0f, 5000.0f);
	const float atk_coef = std::exp(-frame_sec / (atk * 0.001f));
	const float dec_coef = std::exp(-frame_sec / (dec * 0.001f));

	if (raw_energy > envelope_)
		envelope_ = atk_coef * envelope_ + (1.0f - atk_coef) * raw_energy;
	else
		envelope_ = dec_coef * envelope_ + (1.0f - dec_coef) * raw_energy;

	// --- Apply threshold and response curve ---
	const float thr  = clampf((float)threshold.GetValue(frame_number), 0.0f, 1.0f);
	float energy = envelope_;
	if (energy <= thr) {
		energy = 0.0f;
	} else {
		energy = clampf((energy - thr) / std::max(0.001f, 1.0f - thr), 0.0f, 1.0f);
	}
	if (invert)
		energy = 1.0f - energy;

	const float response = clampf(response_curve.Sample(energy, frame_number), 0.0f, 1.0f);
	const int blend = clampi(static_cast<int>(response * 256.0f + 0.5f), 0, 256);
	const int inv = 256 - blend;
	auto out = std::make_shared<QImage>(width, height, QImage::Format_RGBA8888_Premultiplied);
	out->fill(QColor(
		blend_channel(low_color.red.GetInt(frame_number), high_color.red.GetInt(frame_number), inv, blend),
		blend_channel(low_color.green.GetInt(frame_number), high_color.green.GetInt(frame_number), inv, blend),
		blend_channel(low_color.blue.GetInt(frame_number), high_color.blue.GetInt(frame_number), inv, blend),
		blend_channel(low_color.alpha.GetInt(frame_number), high_color.alpha.GetInt(frame_number), inv, blend)));

	frame->AddImage(out);
	return frame;
}

std::string BeatSync::Json() const {
	return JsonValue().toStyledString();
}

Json::Value BeatSync::JsonValue() const {
	Json::Value root = EffectBase::JsonValue();
	root["type"]           = info.class_name;
	root["low_color"]      = low_color.JsonValue();
	root["high_color"]     = high_color.JsonValue();
	root["intensity"]      = intensity.JsonValue();
	root["threshold"]      = threshold.JsonValue();
	root["attack_ms"]      = attack_ms.JsonValue();
	root["decay_ms"]       = decay_ms.JsonValue();
	root["frequency_low"]  = frequency_low.JsonValue();
	root["frequency_high"] = frequency_high.JsonValue();
	root["invert"]         = invert;
	root["response_curve"] = response_curve.JsonValue();
	return root;
}

void BeatSync::SetJson(const std::string value) {
	try {
		const Json::Value root = openshot::stringToJson(value);
		SetJsonValue(root);
	} catch (const std::exception& e) {
		throw InvalidJSON("JSON is invalid (missing keys or invalid data types)");
	}
}

void BeatSync::SetJsonValue(const Json::Value root) {
	EffectBase::SetJsonValue(root);
	if (!root["low_color"].isNull())      low_color.SetJsonValue(root["low_color"]);
	if (!root["high_color"].isNull())     high_color.SetJsonValue(root["high_color"]);
	if (!root["color"].isNull())          high_color.SetJsonValue(root["color"]);
	if (!root["intensity"].isNull())      intensity.SetJsonValue(root["intensity"]);
	if (!root["threshold"].isNull())      threshold.SetJsonValue(root["threshold"]);
	if (!root["attack_ms"].isNull())      attack_ms.SetJsonValue(root["attack_ms"]);
	if (!root["decay_ms"].isNull())       decay_ms.SetJsonValue(root["decay_ms"]);
	if (!root["frequency_low"].isNull())  frequency_low.SetJsonValue(root["frequency_low"]);
	if (!root["frequency_high"].isNull()) frequency_high.SetJsonValue(root["frequency_high"]);
	if (!root["invert"].isNull())         invert = root["invert"].asBool();
	if (!root["response_curve"].isNull()) response_curve.SetJsonValue(root["response_curve"]);
}

std::string BeatSync::PropertiesJSON(int64_t requested_frame) const {
	Json::Value root = BasePropertiesJSON(requested_frame);

	root["low_color"] = add_property_json("Low Color", 0.0, "color", "", &low_color.red, 0, 255, false, requested_frame);
	root["low_color"]["red"]   = add_property_json("Red",   low_color.red.GetValue(requested_frame),   "float", "", &low_color.red,   0, 255, false, requested_frame);
	root["low_color"]["green"] = add_property_json("Green", low_color.green.GetValue(requested_frame), "float", "", &low_color.green, 0, 255, false, requested_frame);
	root["low_color"]["blue"]  = add_property_json("Blue",  low_color.blue.GetValue(requested_frame),  "float", "", &low_color.blue,  0, 255, false, requested_frame);
	root["low_color"]["alpha"] = add_property_json("Alpha", low_color.alpha.GetValue(requested_frame), "float", "", &low_color.alpha, 0, 255, false, requested_frame);

	root["high_color"] = add_property_json("High Color", 0.0, "color", "", &high_color.red, 0, 255, false, requested_frame);
	root["high_color"]["red"]   = add_property_json("Red",   high_color.red.GetValue(requested_frame),   "float", "", &high_color.red,   0, 255, false, requested_frame);
	root["high_color"]["green"] = add_property_json("Green", high_color.green.GetValue(requested_frame), "float", "", &high_color.green, 0, 255, false, requested_frame);
	root["high_color"]["blue"]  = add_property_json("Blue",  high_color.blue.GetValue(requested_frame),  "float", "", &high_color.blue,  0, 255, false, requested_frame);
	root["high_color"]["alpha"] = add_property_json("Alpha", high_color.alpha.GetValue(requested_frame), "float", "", &high_color.alpha, 0, 255, false, requested_frame);

	root["intensity"]   = add_property_json("Intensity",   intensity.GetValue(requested_frame),   "float", "", &intensity,   0.0, 10.0, false, requested_frame);
	root["threshold"]   = add_property_json("Threshold",   threshold.GetValue(requested_frame),   "float", "", &threshold,   0.0,  1.0, false, requested_frame);
	root["attack_ms"]   = add_property_json("Attack (ms)", attack_ms.GetValue(requested_frame),   "float", "", &attack_ms,   1.0, 500.0, false, requested_frame);
	root["decay_ms"]    = add_property_json("Decay (ms)",  decay_ms.GetValue(requested_frame),    "float", "", &decay_ms,    1.0, 2000.0, false, requested_frame);

	root["frequency_low"]  = add_property_json("Low Frequency",  hz_to_normalized_frequency(frequency_low.GetValue(requested_frame)),  "float", "Normalized frequency floor: 0 = 20 Hz, 1 = 20 kHz",    &frequency_low,  0.0, 1.0, false, requested_frame);
	root["frequency_high"] = add_property_json("High Frequency", hz_to_normalized_frequency(frequency_high.GetValue(requested_frame)), "float", "Normalized frequency ceiling: 0 = 20 Hz, 1 = 20 kHz", &frequency_high, 0.0, 1.0, false, requested_frame);

	root["invert"] = add_property_json("Invert", invert ? 1.0 : 0.0, "int", "", NULL, 0, 1, false, requested_frame);
	root["invert"]["choices"].append(add_property_choice_json("No",  0, invert ? 1 : 0));
	root["invert"]["choices"].append(add_property_choice_json("Yes", 1, invert ? 1 : 0));

	root["response_curve"] = add_property_json("Response Curve", 0.0, "colorgrade_curve", response_curve.Summary(requested_frame), NULL, 0.0, 1.0, false, requested_frame);
	root["response_curve"]["curve"] = response_curve.JsonValue();
	root["response_curve"]["channel"] = "all";
	root["response_curve"]["summary"] = response_curve.Summary(requested_frame);

	return root.toStyledString();
}
