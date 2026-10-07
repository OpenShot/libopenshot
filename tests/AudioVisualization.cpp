/**
 * @file
 * @brief Unit tests for openshot::AudioVisualization effect
 * @author OpenAI
 *
 * @ref License
 */

// Copyright (c) 2008-2026 OpenShot Studios, LLC
//
// SPDX-License-Identifier: LGPL-3.0-or-later

#include <cmath>
#include <memory>
#include <vector>

#include <QColor>
#include <QImage>

#include "EffectInfo.h"
#include "Frame.h"
#include "Timeline.h"
#include "effects/AudioVisualization.h"
#include "openshot_catch.h"

using namespace openshot;

namespace {
	constexpr double PI = 3.14159265358979323846;

	std::shared_ptr<Frame> make_audio_frame() {
		const int sample_rate = 48000;
		const int samples = 1600;
		auto frame = std::make_shared<Frame>(1, 320, 180, "#00000000", samples, 2);
		frame->AddImage(std::make_shared<QImage>(320, 180, QImage::Format_RGBA8888_Premultiplied));
		frame->GetImage()->fill(Qt::transparent);
		frame->ResizeAudio(2, samples, sample_rate, LAYOUT_STEREO);

		std::vector<float> left(samples);
		std::vector<float> right(samples);
		for (int i = 0; i < samples; ++i) {
			left[i] = std::sin(2.0 * PI * 440.0 * i / sample_rate) * 0.7f;
			right[i] = std::sin(2.0 * PI * 660.0 * i / sample_rate) * 0.5f;
		}
		frame->AddAudio(true, 0, 0, left.data(), samples, 1.0f);
		frame->AddAudio(true, 1, 0, right.data(), samples, 1.0f);
		return frame;
	}

	int count_visible_pixels(const QImage& image) {
		int visible = 0;
		for (int y = 0; y < image.height(); ++y) {
			for (int x = 0; x < image.width(); ++x) {
				if (image.pixelColor(x, y).alpha() > 0)
					++visible;
			}
		}
		return visible;
	}

	int count_visible_non_red_pixels(const QImage& image) {
		int visible = 0;
		for (int y = 0; y < image.height(); ++y) {
			for (int x = 0; x < image.width(); ++x) {
				const QColor pixel = image.pixelColor(x, y);
				if (pixel.alpha() > 0 && (pixel.green() > pixel.red() || pixel.blue() > pixel.red()))
					++visible;
			}
		}
		return visible;
	}
}

TEST_CASE("AudioVisualization is registered", "[effect][audio-visualization]")
{
	std::unique_ptr<EffectBase> effect(EffectInfo().CreateEffect("AudioVisualization"));
	REQUIRE(effect != nullptr);
	CHECK(effect->JsonValue()["type"].asString() == "AudioVisualization");
	CHECK(effect->JsonValue()["apply_before_clip"].asBool() == true);
	CHECK(effect->JsonValue()["visualization_type"].asInt() == AUDIO_VISUALIZATION_WAVEFORM);
	CHECK(effect->JsonValue()["channel_layout"].asInt() == AUDIO_VISUALIZATION_CHANNEL_AUTO);
	CHECK(effect->JsonValue()["background"].asInt() == AUDIO_VISUALIZATION_BACKGROUND_SOURCE);
}

TEST_CASE("AudioVisualization renders transparent visual modes", "[effect][audio-visualization]")
{
	for (int mode = AUDIO_VISUALIZATION_WAVEFORM; mode <= AUDIO_VISUALIZATION_RADIAL_BARS; ++mode) {
		auto frame = make_audio_frame();
		AudioVisualization effect;
		effect.visualization_type = mode;
		effect.color = Color((unsigned char)255, (unsigned char)64, (unsigned char)32, (unsigned char)255);
		effect.glow = Keyframe(0.0);
		effect.detail = Keyframe(0.4);
		effect.background = AUDIO_VISUALIZATION_BACKGROUND_TRANSPARENT;

		auto out = effect.GetFrame(frame, 1)->GetImage();
		REQUIRE(out != nullptr);
		CHECK(out->width() == 320);
		CHECK(out->height() == 180);
		CHECK(count_visible_pixels(*out) > 0);
		CHECK(count_visible_pixels(*out) < out->width() * out->height());
	}
}

TEST_CASE("AudioVisualization uses timeline canvas for audio-only frames", "[effect][audio-visualization]")
{
	Timeline timeline(640, 360, Fraction(30, 1), 48000, 2, LAYOUT_STEREO);
	auto frame = make_audio_frame();
	frame->AddImage(std::make_shared<QImage>(1, 1, QImage::Format_RGBA8888_Premultiplied));
	frame->GetImage()->fill(Qt::transparent);

	AudioVisualization effect;
	effect.ParentTimeline(&timeline);
	effect.visualization_type = AUDIO_VISUALIZATION_BARS;
	effect.background = AUDIO_VISUALIZATION_BACKGROUND_TRANSPARENT;

	auto out = effect.GetFrame(frame, 1)->GetImage();
	REQUIRE(out != nullptr);
	CHECK(out->width() == 640);
	CHECK(out->height() == 360);
	CHECK(count_visible_pixels(*out) > 0);
}

TEST_CASE("AudioVisualization differentiates combined and overlay waveform channels", "[effect][audio-visualization]")
{
	AudioVisualization combined;
	combined.visualization_type = AUDIO_VISUALIZATION_WAVEFORM;
	combined.channel_layout = AUDIO_VISUALIZATION_CHANNEL_COMBINED;
	combined.glow = Keyframe(0.0);

	AudioVisualization overlay;
	overlay.visualization_type = AUDIO_VISUALIZATION_WAVEFORM;
	overlay.channel_layout = AUDIO_VISUALIZATION_CHANNEL_OVERLAY;
	overlay.glow = Keyframe(0.0);

	auto combined_image = combined.GetFrame(make_audio_frame(), 1)->GetImage()->copy();
	auto overlay_image = overlay.GetFrame(make_audio_frame(), 1)->GetImage()->copy();

	CHECK(combined_image != overlay_image);
}

TEST_CASE("AudioVisualization phase scope supports rainbow color mode", "[effect][audio-visualization]")
{
	AudioVisualization effect;
	effect.visualization_type = AUDIO_VISUALIZATION_PHASE_SCOPE;
	effect.color_mode = AUDIO_VISUALIZATION_COLOR_RAINBOW;
	effect.color = Color((unsigned char)255, (unsigned char)0, (unsigned char)0, (unsigned char)255);
	effect.glow = Keyframe(0.0);
	effect.detail = Keyframe(1.0);
	effect.background = AUDIO_VISUALIZATION_BACKGROUND_TRANSPARENT;

	auto out = effect.GetFrame(make_audio_frame(), 1)->GetImage();
	REQUIRE(out != nullptr);
	CHECK(count_visible_non_red_pixels(*out) > 0);
}

TEST_CASE("AudioVisualization exposes normalized frequency controls", "[effect][audio-visualization]")
{
	AudioVisualization effect;
	const Json::Value props = openshot::stringToJson(effect.PropertiesJSON(1));

	CHECK(props["color_spread"]["value"].asFloat() == 0.6f);
	CHECK(props["color_spread"]["min"].asFloat() == 0.0f);
	CHECK(props["color_spread"]["max"].asFloat() == 1.0f);
	CHECK(props["frequency_low"]["value"].asFloat() == 0.0f);
	CHECK(props["frequency_low"]["min"].asFloat() == 0.0f);
	CHECK(props["frequency_low"]["max"].asFloat() == 1.0f);
	CHECK(props["frequency_high"]["value"].asFloat() == 1.0f);
	CHECK(props["frequency_high"]["min"].asFloat() == 0.0f);
	CHECK(props["frequency_high"]["max"].asFloat() == 1.0f);
}

TEST_CASE("AudioVisualization JSON roundtrip preserves visualization controls", "[effect][audio-visualization][json]")
{
	AudioVisualization effect;
	effect.visualization_type = AUDIO_VISUALIZATION_PARTICLES;
	effect.style = AUDIO_VISUALIZATION_STYLE_NEON;
	effect.color = Color((unsigned char)20, (unsigned char)180, (unsigned char)240, (unsigned char)220);
	effect.intensity = Keyframe(1.75);
	effect.smoothing = Keyframe(0.2);
	effect.detail = Keyframe(0.9);
	effect.glow = Keyframe(0.65);
	effect.color_spread = Keyframe(0.25);
	effect.color_mode = AUDIO_VISUALIZATION_COLOR_RAINBOW;
	effect.channel_layout = AUDIO_VISUALIZATION_CHANNEL_OVERLAY;
	effect.frequency_low = Keyframe(0.18);
	effect.frequency_high = Keyframe(0.82);
	effect.background = AUDIO_VISUALIZATION_BACKGROUND_GRADIENT;

	AudioVisualization copy;
	copy.SetJsonValue(effect.JsonValue());

	CHECK(copy.visualization_type == AUDIO_VISUALIZATION_PARTICLES);
	CHECK(copy.style == AUDIO_VISUALIZATION_STYLE_NEON);
	CHECK(copy.color_mode == AUDIO_VISUALIZATION_COLOR_RAINBOW);
	CHECK(copy.channel_layout == AUDIO_VISUALIZATION_CHANNEL_OVERLAY);
	CHECK(copy.background == AUDIO_VISUALIZATION_BACKGROUND_GRADIENT);
	CHECK(copy.color.red.GetValue(1) == Approx(20.0).margin(0.001));
	CHECK(copy.color.green.GetValue(1) == Approx(180.0).margin(0.001));
	CHECK(copy.color.blue.GetValue(1) == Approx(240.0).margin(0.001));
	CHECK(copy.color.alpha.GetValue(1) == Approx(220.0).margin(0.001));
	CHECK(copy.intensity.GetValue(1) == Approx(1.75).margin(0.001));
	CHECK(copy.smoothing.GetValue(1) == Approx(0.2).margin(0.001));
	CHECK(copy.detail.GetValue(1) == Approx(0.9).margin(0.001));
	CHECK(copy.glow.GetValue(1) == Approx(0.65).margin(0.001));
	CHECK(copy.color_spread.GetValue(1) == Approx(0.25).margin(0.001));
	CHECK(copy.frequency_low.GetValue(1) == Approx(0.18).margin(0.001));
	CHECK(copy.frequency_high.GetValue(1) == Approx(0.82).margin(0.001));
}

TEST_CASE("AudioVisualization source background preserves input image", "[effect][audio-visualization]")
{
	auto frame = make_audio_frame();
	auto source = std::make_shared<QImage>(320, 180, QImage::Format_RGBA8888_Premultiplied);
	source->fill(QColor(12, 34, 56, 255));
	frame->AddImage(source);

	AudioVisualization effect;
	effect.visualization_type = AUDIO_VISUALIZATION_WAVEFORM;
	effect.background = AUDIO_VISUALIZATION_BACKGROUND_SOURCE;

	auto out = effect.GetFrame(frame, 1)->GetImage();
	REQUIRE(out != nullptr);
	CHECK(out->pixelColor(0, 0) == QColor(12, 34, 56, 255));
}
