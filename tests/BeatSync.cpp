/**
 * @file
 * @brief Unit tests for openshot::BeatSync effect
 * @author Jonathan Thomas <jonathan@openshot.org>
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
#include "effects/BeatSync.h"
#include "openshot_catch.h"

using namespace openshot;

namespace {
	constexpr double PI = 3.14159265358979323846;

	// Frame with a solid-color image and a sine-wave audio signal at the given amplitude
	std::shared_ptr<Frame> make_frame(int64_t frame_number, QColor fill_color, float audio_amplitude,
		int width = 320, int height = 180, float freq_hz = 440.0f) {
		const int sample_rate = 48000;
		const int samples = 1600;
		auto frame = std::make_shared<Frame>(frame_number, width, height, "#00000000", samples, 2);
		auto img = std::make_shared<QImage>(width, height, QImage::Format_RGBA8888_Premultiplied);
		img->fill(fill_color);
		frame->AddImage(img);
		frame->ResizeAudio(2, samples, sample_rate, LAYOUT_STEREO);
		std::vector<float> audio(samples);
		for (int i = 0; i < samples; ++i)
			audio[i] = audio_amplitude * std::sin(2.0 * PI * freq_hz * i / sample_rate);
		frame->AddAudio(true, 0, 0, audio.data(), samples, 1.0f);
		frame->AddAudio(true, 1, 0, audio.data(), samples, 1.0f);
		return frame;
	}

	// Frame with no audio samples at all
	std::shared_ptr<Frame> make_silent_frame(QColor fill_color = Qt::black) {
		const int sample_rate = 48000;
		const int samples = 1600;
		auto frame = std::make_shared<Frame>(1, 320, 180, "#00000000", samples, 2);
		auto img = std::make_shared<QImage>(320, 180, QImage::Format_RGBA8888_Premultiplied);
		img->fill(fill_color);
		frame->AddImage(img);
		frame->ResizeAudio(2, samples, sample_rate, LAYOUT_STEREO);
		// Leave audio zeroed out — silence
		std::vector<float> zeros(samples, 0.0f);
		frame->AddAudio(true, 0, 0, zeros.data(), samples, 1.0f);
		frame->AddAudio(true, 1, 0, zeros.data(), samples, 1.0f);
		return frame;
	}

	QColor sample_pixel(const std::shared_ptr<Frame>& frame, int x, int y) {
		return frame->GetImage()->pixelColor(x, y);
	}

	// Average luminance across the center of the image
	float average_luminance(const std::shared_ptr<Frame>& frame) {
		const QImage& img = *frame->GetImage();
		double total = 0.0;
		const int cx = img.width() / 2, cy = img.height() / 2;
		const int size = 20;
		int count = 0;
		for (int y = cy - size; y < cy + size; ++y) {
			for (int x = cx - size; x < cx + size; ++x) {
				if (x < 0 || y < 0 || x >= img.width() || y >= img.height()) continue;
				QColor c = img.pixelColor(x, y);
				total += (c.red() + c.green() + c.blue()) / 3.0;
				++count;
			}
		}
		return count > 0 ? static_cast<float>(total / count) : 0.0f;
	}
}

// ── Registration ─────────────────────────────────────────────────────────────

TEST_CASE("BeatSync is registered in EffectInfo", "[effect][beat-sync]")
{
	std::unique_ptr<EffectBase> effect(EffectInfo().CreateEffect("BeatSync"));
	REQUIRE(effect != nullptr);
	const Json::Value props = effect->JsonValue();
	CHECK(props["type"].asString() == "BeatSync");
}

// ── Defaults ─────────────────────────────────────────────────────────────────

TEST_CASE("BeatSync default parameters are sane", "[effect][beat-sync]")
{
	BeatSync effect;
	CHECK(effect.low_color.red.GetValue(1)     == Approx(0.0).margin(0.01));
	CHECK(effect.low_color.green.GetValue(1)   == Approx(0.0).margin(0.01));
	CHECK(effect.low_color.blue.GetValue(1)    == Approx(0.0).margin(0.01));
	CHECK(effect.low_color.alpha.GetValue(1)   == Approx(255.0).margin(0.01));
	CHECK(effect.high_color.red.GetValue(1)    == Approx(255.0).margin(0.01));
	CHECK(effect.high_color.green.GetValue(1)  == Approx(255.0).margin(0.01));
	CHECK(effect.high_color.blue.GetValue(1)   == Approx(255.0).margin(0.01));
	CHECK(effect.high_color.alpha.GetValue(1)  == Approx(255.0).margin(0.01));
	CHECK(effect.intensity.GetValue(1)   == Approx(2.0).margin(0.01));
	CHECK(effect.threshold.GetValue(1)   == Approx(0.1).margin(0.01));
	CHECK(effect.attack_ms.GetValue(1)   == Approx(10.0).margin(0.01));
	CHECK(effect.decay_ms.GetValue(1)    == Approx(200.0).margin(0.01));
	CHECK(effect.frequency_low.GetValue(1)  == Approx(0.0).margin(0.01));
	CHECK(effect.frequency_high.GetValue(1) == Approx(1.0).margin(0.01));
	CHECK(effect.response_curve.Sample(0.5f, 1) == Approx(0.5f).margin(0.01));
	CHECK(effect.invert == false);
}

// ── Output size ──────────────────────────────────────────────────────────────

TEST_CASE("BeatSync output matches input frame size", "[effect][beat-sync]")
{
	BeatSync effect;
	effect.threshold = Keyframe(0.0);
	auto out = effect.GetFrame(make_frame(1, Qt::black, 0.8f), 1);
	REQUIRE(out->GetImage() != nullptr);
	CHECK(out->GetImage()->width()  == 320);
	CHECK(out->GetImage()->height() == 180);
}

TEST_CASE("BeatSync uses timeline canvas when frame image is 1x1", "[effect][beat-sync]")
{
	Timeline timeline(640, 360, Fraction(30, 1), 48000, 2, LAYOUT_STEREO);

	const int sample_rate = 48000;
	const int samples = 1600;
	auto frame = std::make_shared<Frame>(1, 640, 360, "#00000000", samples, 2);
	auto tiny = std::make_shared<QImage>(1, 1, QImage::Format_RGBA8888_Premultiplied);
	tiny->fill(Qt::black);
	frame->AddImage(tiny);
	frame->ResizeAudio(2, samples, sample_rate, LAYOUT_STEREO);
	std::vector<float> audio(samples, 0.8f);
	frame->AddAudio(true, 0, 0, audio.data(), samples, 1.0f);
	frame->AddAudio(true, 1, 0, audio.data(), samples, 1.0f);

	BeatSync effect;
	effect.threshold = Keyframe(0.0);
	effect.ParentTimeline(&timeline);

	auto out = effect.GetFrame(frame, 1)->GetImage();
	REQUIRE(out != nullptr);
	CHECK(out->width()  == 640);
	CHECK(out->height() == 360);
}

// ── Core pixel behavior ───────────────────────────────────────────────────────

TEST_CASE("BeatSync generates high color on loud audio", "[effect][beat-sync]")
{
	BeatSync effect;
	effect.threshold   = Keyframe(0.0);
	effect.intensity   = Keyframe(10.0);
	effect.attack_ms   = Keyframe(1.0);
	effect.decay_ms    = Keyframe(1.0);

	auto out = effect.GetFrame(make_frame(1, Qt::red, 0.9f), 1);
	const float luma = average_luminance(out);
	CHECK(luma > 200.0f);
}

TEST_CASE("BeatSync generates low color on silence", "[effect][beat-sync]")
{
	BeatSync effect;
	effect.threshold = Keyframe(0.05);

	auto out = effect.GetFrame(make_silent_frame(Qt::red), 1);
	const float luma = average_luminance(out);
	CHECK(luma < 2.0f);
}

TEST_CASE("BeatSync can generate white to black flashes", "[effect][beat-sync]")
{
	BeatSync effect;
	effect.low_color   = Color((unsigned char)255, (unsigned char)255, (unsigned char)255, (unsigned char)255);
	effect.high_color  = Color((unsigned char)0, (unsigned char)0, (unsigned char)0, (unsigned char)255);
	effect.threshold   = Keyframe(0.0);
	effect.intensity   = Keyframe(10.0);
	effect.attack_ms   = Keyframe(1.0);
	effect.decay_ms    = Keyframe(1.0);

	auto out = effect.GetFrame(make_frame(1, Qt::red, 0.9f), 1);
	const float luma = average_luminance(out);
	CHECK(luma < 55.0f);
}

TEST_CASE("BeatSync louder audio produces more effect than quieter audio", "[effect][beat-sync]")
{
	// intensity=0.2 keeps even loud tones below full saturation so levels are distinguishable
	auto run = [](float amplitude) {
		BeatSync effect;
		effect.threshold   = Keyframe(0.0);
		effect.intensity   = Keyframe(0.2);
		effect.attack_ms   = Keyframe(1.0);
		effect.decay_ms    = Keyframe(1.0);
		return average_luminance(effect.GetFrame(make_frame(1, Qt::red, amplitude), 1));
	};

	CHECK(run(0.8f) > run(0.2f));
	CHECK(run(0.2f) > run(0.05f));
}

TEST_CASE("BeatSync ignores source footage color", "[effect][beat-sync]")
{
	BeatSync effect_a, effect_b;
	effect_a.threshold = effect_b.threshold = Keyframe(0.0);
	effect_a.intensity = effect_b.intensity = Keyframe(0.5);
	effect_a.attack_ms = effect_b.attack_ms = Keyframe(1.0);
	effect_a.decay_ms  = effect_b.decay_ms  = Keyframe(1.0);

	const QColor out_a = effect_a.GetFrame(make_frame(1, Qt::red,  0.5f), 1)->GetImage()->pixelColor(160, 90);
	const QColor out_b = effect_b.GetFrame(make_frame(1, Qt::blue, 0.5f), 1)->GetImage()->pixelColor(160, 90);
	CHECK(out_a == out_b);
}

// ── Alpha generation ─────────────────────────────────────────────────────────

TEST_CASE("BeatSync interpolates configured alpha", "[effect][beat-sync]")
{
	BeatSync effect;
	effect.low_color  = Color((unsigned char)0, (unsigned char)0, (unsigned char)0, (unsigned char)0);
	effect.high_color = Color((unsigned char)255, (unsigned char)255, (unsigned char)255, (unsigned char)255);
	effect.threshold  = Keyframe(0.0);
	effect.intensity  = Keyframe(0.2);
	effect.attack_ms  = Keyframe(1.0);
	effect.decay_ms   = Keyframe(1.0);

	const int quiet_alpha = sample_pixel(effect.GetFrame(make_frame(1, Qt::red, 0.2f), 1), 160, 90).alpha();
	BeatSync loud_effect;
	loud_effect.low_color  = Color((unsigned char)0, (unsigned char)0, (unsigned char)0, (unsigned char)0);
	loud_effect.high_color = Color((unsigned char)255, (unsigned char)255, (unsigned char)255, (unsigned char)255);
	loud_effect.threshold  = Keyframe(0.0);
	loud_effect.intensity  = Keyframe(0.2);
	loud_effect.attack_ms  = Keyframe(1.0);
	loud_effect.decay_ms   = Keyframe(1.0);
	const int loud_alpha = sample_pixel(loud_effect.GetFrame(make_frame(1, Qt::red, 0.8f), 1), 160, 90).alpha();
	CHECK(loud_alpha > quiet_alpha);
}

TEST_CASE("BeatSync response curve shapes generated color blend", "[effect][beat-sync][curve]")
{
	auto run = [](bool boost_mid) {
		BeatSync effect;
		effect.threshold = Keyframe(0.0);
		effect.intensity = Keyframe(0.2);
		effect.attack_ms = Keyframe(1.0);
		effect.decay_ms  = Keyframe(1.0);
		if (boost_mid) {
			effect.response_curve.Nodes().clear();
			effect.response_curve.Nodes().push_back(AnimatedCurveNode(10, 0.0, 0.0, LINEAR));
			effect.response_curve.Nodes().push_back(AnimatedCurveNode(20, 0.5, 0.9, LINEAR));
			effect.response_curve.Nodes().push_back(AnimatedCurveNode(30, 1.0, 1.0, LINEAR));
		}
		return average_luminance(effect.GetFrame(make_frame(1, Qt::red, 0.4f), 1));
	};

	CHECK(run(true) > run(false));
}

// ── Invert mode ───────────────────────────────────────────────────────────────

TEST_CASE("BeatSync invert=true applies maximum effect on silence", "[effect][beat-sync]")
{
	BeatSync effect;
	effect.invert    = true;
	effect.threshold = Keyframe(0.0);
	effect.intensity = Keyframe(2.0);

	// Silence -> energy=0 -> inverted to 1.0 -> full high color
	auto out = effect.GetFrame(make_silent_frame(Qt::black), 1);
	const float luma = average_luminance(out);
	CHECK(luma > 200.0f);
}

TEST_CASE("BeatSync invert=true reduces effect on loud audio", "[effect][beat-sync]")
{
	auto run_inverted = [](float amplitude) {
		BeatSync effect;
		effect.invert    = true;
		effect.threshold = Keyframe(0.0);
		effect.intensity = Keyframe(5.0);
		effect.attack_ms = Keyframe(1.0);
		effect.decay_ms  = Keyframe(1.0);
		return average_luminance(effect.GetFrame(make_frame(1, Qt::black, amplitude), 1));
	};

	// Louder audio → energy closer to 1 → inverted closer to 0 → less effect
	CHECK(run_inverted(0.9f) < run_inverted(0.05f));
}

// ── Frequency filtering ───────────────────────────────────────────────────────

TEST_CASE("BeatSync bass band detects low-frequency tone", "[effect][beat-sync]")
{
	// 80 Hz tone — should be captured by bass band, attenuated in highs band
	const float bass_low  = 0.03f;  // ~20 Hz
	const float bass_high = 0.25f;  // ~250 Hz
	const float hi_low    = 0.72f;  // ~6 kHz
	const float hi_high   = 1.0f;   // 20 kHz

	auto run_band = [](float freq_hz, float band_lo, float band_hi) {
		BeatSync effect;
		effect.frequency_low  = Keyframe(band_lo);
		effect.frequency_high = Keyframe(band_hi);
		effect.threshold   = Keyframe(0.0);
		effect.intensity   = Keyframe(5.0);
		effect.attack_ms   = Keyframe(1.0);
		effect.decay_ms    = Keyframe(1.0);
		return average_luminance(effect.GetFrame(make_frame(1, Qt::black, 0.9f, 320, 180, freq_hz), 1));
	};

	// An 80 Hz tone should trigger more effect in the bass band than the highs band
	CHECK(run_band(80.0f, bass_low, bass_high) > run_band(80.0f, hi_low, hi_high));
}

TEST_CASE("BeatSync high band detects high-frequency tone", "[effect][beat-sync]")
{
	const float bass_low  = 0.03f;
	const float bass_high = 0.25f;
	const float hi_low    = 0.72f;
	const float hi_high   = 1.0f;

	auto run_band = [](float freq_hz, float band_lo, float band_hi) {
		BeatSync effect;
		effect.frequency_low  = Keyframe(band_lo);
		effect.frequency_high = Keyframe(band_hi);
		effect.threshold   = Keyframe(0.0);
		effect.intensity   = Keyframe(5.0);
		effect.attack_ms   = Keyframe(1.0);
		effect.decay_ms    = Keyframe(1.0);
		return average_luminance(effect.GetFrame(make_frame(1, Qt::black, 0.9f, 320, 180, freq_hz), 1));
	};

	// An 8 kHz tone should trigger more effect in the highs band than the bass band
	CHECK(run_band(8000.0f, hi_low, hi_high) > run_band(8000.0f, bass_low, bass_high));
}

// ── Envelope behavior ─────────────────────────────────────────────────────────

TEST_CASE("BeatSync envelope decays over silent frames", "[effect][beat-sync]")
{
	BeatSync effect;
	effect.threshold = Keyframe(0.0);
	effect.intensity = Keyframe(10.0);
	effect.attack_ms = Keyframe(1.0);
	effect.decay_ms  = Keyframe(50.0);

	// Prime the envelope with a loud frame, then let it decay through silence
	effect.GetFrame(make_frame(1, Qt::black, 0.9f), 1);
	const float luma_after_loud = average_luminance(effect.GetFrame(make_silent_frame(), 2));

	for (int i = 3; i <= 20; ++i)
		effect.GetFrame(make_silent_frame(), i);
	const float luma_after_decay = average_luminance(effect.GetFrame(make_silent_frame(), 21));

	CHECK(luma_after_decay < luma_after_loud);
}

TEST_CASE("BeatSync envelope resets on seek", "[effect][beat-sync]")
{
	BeatSync effect;
	effect.threshold = Keyframe(0.0);
	effect.intensity = Keyframe(10.0);
	effect.attack_ms = Keyframe(1.0);
	effect.decay_ms  = Keyframe(5000.0);  // very slow decay

	// Build up envelope
	effect.GetFrame(make_frame(1, Qt::black, 0.9f), 1);

	// Seek far forward (discontinuous), envelope should reset
	const float luma_post_seek = average_luminance(effect.GetFrame(make_silent_frame(), 100));
	CHECK(luma_post_seek < 5.0f);
}

// ── JSON round-trip ───────────────────────────────────────────────────────────

TEST_CASE("BeatSync JSON round-trip preserves all properties", "[effect][beat-sync][json]")
{
	BeatSync effect;
	effect.low_color   = Color((unsigned char)10, (unsigned char)20, (unsigned char)30, (unsigned char)40);
	effect.high_color  = Color((unsigned char)200, (unsigned char)50, (unsigned char)100, (unsigned char)180);
	effect.intensity   = Keyframe(3.5);
	effect.threshold   = Keyframe(0.15);
	effect.attack_ms   = Keyframe(25.0);
	effect.decay_ms    = Keyframe(300.0);
	effect.frequency_low  = Keyframe(0.12);
	effect.frequency_high = Keyframe(0.78);
	effect.invert      = true;
	effect.response_curve.Nodes().clear();
	effect.response_curve.Nodes().push_back(AnimatedCurveNode(10, 0.0, 0.0, LINEAR));
	effect.response_curve.Nodes().push_back(AnimatedCurveNode(20, 0.5, 0.9, LINEAR));
	effect.response_curve.Nodes().push_back(AnimatedCurveNode(30, 1.0, 1.0, LINEAR));

	BeatSync copy;
	copy.SetJsonValue(effect.JsonValue());

	CHECK(copy.low_color.red.GetValue(1)   == Approx(10.0).margin(0.01));
	CHECK(copy.low_color.green.GetValue(1) == Approx(20.0).margin(0.01));
	CHECK(copy.low_color.blue.GetValue(1)  == Approx(30.0).margin(0.01));
	CHECK(copy.low_color.alpha.GetValue(1) == Approx(40.0).margin(0.01));
	CHECK(copy.high_color.red.GetValue(1)   == Approx(200.0).margin(0.01));
	CHECK(copy.high_color.green.GetValue(1) == Approx(50.0).margin(0.01));
	CHECK(copy.high_color.blue.GetValue(1)  == Approx(100.0).margin(0.01));
	CHECK(copy.high_color.alpha.GetValue(1) == Approx(180.0).margin(0.01));
	CHECK(copy.intensity.GetValue(1)   == Approx(3.5).margin(0.001));
	CHECK(copy.threshold.GetValue(1)   == Approx(0.15).margin(0.001));
	CHECK(copy.attack_ms.GetValue(1)   == Approx(25.0).margin(0.001));
	CHECK(copy.decay_ms.GetValue(1)    == Approx(300.0).margin(0.001));
	CHECK(copy.frequency_low.GetValue(1)  == Approx(0.12).margin(0.001));
	CHECK(copy.frequency_high.GetValue(1) == Approx(0.78).margin(0.001));
	CHECK(copy.invert      == true);
	CHECK(copy.response_curve.Nodes().size() == 3);
	CHECK(copy.response_curve.Sample(0.5f, 1) == Approx(0.9f).margin(0.02f));
}

TEST_CASE("BeatSync SetJson string round-trip works", "[effect][beat-sync][json]")
{
	BeatSync effect;
	effect.intensity  = Keyframe(4.0);
	effect.threshold  = Keyframe(0.2);
	effect.invert     = true;

	BeatSync copy;
	copy.SetJson(effect.Json());

	CHECK(copy.intensity.GetValue(1)  == Approx(4.0).margin(0.001));
	CHECK(copy.threshold.GetValue(1)  == Approx(0.2).margin(0.001));
	CHECK(copy.invert == true);
}

TEST_CASE("BeatSync SetJson maps legacy color to high color", "[effect][beat-sync][json]")
{
	BeatSync legacy;
	legacy.high_color = Color((unsigned char)20, (unsigned char)30, (unsigned char)40, (unsigned char)255);
	Json::Value root = legacy.JsonValue();
	root["color"] = root["high_color"];
	root.removeMember("high_color");

	BeatSync copy;
	copy.SetJsonValue(root);
	CHECK(copy.high_color.red.GetValue(1)   == Approx(20.0).margin(0.01));
	CHECK(copy.high_color.green.GetValue(1) == Approx(30.0).margin(0.01));
	CHECK(copy.high_color.blue.GetValue(1)  == Approx(40.0).margin(0.01));
}

TEST_CASE("BeatSync SetJson throws on invalid JSON", "[effect][beat-sync][json]")
{
	BeatSync effect;
	CHECK_THROWS_AS(effect.SetJson("{not valid json}"), openshot::InvalidJSON);
}

// ── PropertiesJSON ────────────────────────────────────────────────────────────

TEST_CASE("BeatSync PropertiesJSON exposes all expected properties", "[effect][beat-sync]")
{
	BeatSync effect;
	const Json::Value props = openshot::stringToJson(effect.PropertiesJSON(1));

	CHECK(props["intensity"]["value"].asFloat() == Approx(2.0f).margin(0.01f));
	CHECK(props["intensity"]["min"].asFloat()   == Approx(0.0f).margin(0.01f));
	CHECK(props["intensity"]["max"].asFloat()   == Approx(10.0f).margin(0.01f));

	CHECK(props["threshold"]["value"].asFloat() == Approx(0.1f).margin(0.01f));
	CHECK(props["threshold"]["min"].asFloat()   == Approx(0.0f).margin(0.01f));
	CHECK(props["threshold"]["max"].asFloat()   == Approx(1.0f).margin(0.01f));

	CHECK(props["frequency_low"]["value"].asFloat()  == Approx(0.0f).margin(0.01f));
	CHECK(props["frequency_high"]["value"].asFloat() == Approx(1.0f).margin(0.01f));
	CHECK(props["frequency_low"]["min"].asFloat()    == Approx(0.0f).margin(0.01f));
	CHECK(props["frequency_high"]["max"].asFloat()   == Approx(1.0f).margin(0.01f));

	// Invert should expose both choices
	CHECK(props["invert"]["choices"].size() == 2);

	CHECK(props["low_color"]["type"].asString() == "color");
	CHECK(props["high_color"]["type"].asString() == "color");
	CHECK(props["response_curve"]["type"].asString() == "colorgrade_curve");
	CHECK(props["response_curve"]["curve"]["nodes"].size() == 2);
}
