/**
 * @file
 * @brief Unit tests for DenoiseImage effect
 *
 * @ref License
 */

// Copyright (c) 2008-2026 OpenShot Studios, LLC
//
// SPDX-License-Identifier: LGPL-3.0-or-later

#include <cmath>
#include <memory>
#include <sstream>

#include <QColor>
#include <QImage>

#include "EffectInfo.h"
#include "Frame.h"
#include "effects/DenoiseImage.h"
#include "openshot_catch.h"

using namespace openshot;

static std::shared_ptr<Frame> makeDenoiseFrame()
{
	QImage img(5, 5, QImage::Format_ARGB32);
	img.fill(QColor(42, 42, 42, 255));
	img.setPixelColor(2, 2, QColor(84, 20, 90, 128));
	auto frame = std::make_shared<Frame>();
	*frame->GetImage() = img;
	return frame;
}

static std::shared_ptr<Frame> makeFlatFrame(const QColor& color)
{
	QImage img(5, 5, QImage::Format_ARGB32);
	img.fill(color);
	auto frame = std::make_shared<Frame>();
	*frame->GetImage() = img;
	return frame;
}

static std::shared_ptr<Frame> makeNativeDenoiseFrame()
{
	QImage img(5, 5, QImage::Format_RGBA8888_Premultiplied);
	img.fill(QColor(42, 42, 42, 255));
	img.setPixelColor(2, 2, QColor(84, 20, 90, 255));
	auto frame = std::make_shared<Frame>();
	frame->AddImage(std::make_shared<QImage>(img));
	return frame;
}

static AnimatedCurve makeShadowOnlyCurve()
{
	AnimatedCurve curve;
	curve.Nodes().clear();
	curve.Nodes().emplace_back(0, 0.0, 1.0, LINEAR);
	curve.Nodes().emplace_back(1, 0.5, 0.0, LINEAR);
	curve.Nodes().emplace_back(2, 1.0, 0.0, LINEAR);
	return curve;
}

TEST_CASE("DenoiseImage zero strength leaves pixels unchanged", "[effect][denoiseimage]")
{
	DenoiseImage effect;
	effect.strength = Keyframe(0.0);

	auto frame = makeDenoiseFrame();
	const QColor before = frame->GetImage()->pixelColor(2, 2);
	auto out = effect.GetFrame(frame, 1);
	const QColor after = out->GetImage()->pixelColor(2, 2);

	CHECK(after == before);
}

TEST_CASE("DenoiseImage preserves alpha while reducing color speckles", "[effect][denoiseimage]")
{
	DenoiseImage effect;
	effect.strength = Keyframe(1.0);
	effect.detail = Keyframe(0.0);
	effect.temporal = Keyframe(0.0);
	effect.color_noise = Keyframe(1.0);

	auto frame = makeDenoiseFrame();
	const QColor before = frame->GetImage()->pixelColor(2, 2);
	auto out = effect.GetFrame(frame, 1);
	const QColor after = out->GetImage()->pixelColor(2, 2);

	CHECK(after.alpha() == before.alpha());
	const int before_delta = std::abs(before.red() - 42) +
		std::abs(before.green() - 42) +
		std::abs(before.blue() - 42);
	const int after_delta = std::abs(after.red() - 42) +
		std::abs(after.green() - 42) +
		std::abs(after.blue() - 42);
	CHECK(after_delta < before_delta);
}

TEST_CASE("DenoiseImage default settings visibly reduce isolated speckles", "[effect][denoiseimage]")
{
	DenoiseImage effect;

	auto frame = makeDenoiseFrame();
	const QColor before = frame->GetImage()->pixelColor(2, 2);
	auto out = effect.GetFrame(frame, 1);
	const QColor after = out->GetImage()->pixelColor(2, 2);

	const int before_delta = std::abs(before.red() - 42) +
		std::abs(before.green() - 42) +
		std::abs(before.blue() - 42);
	const int after_delta = std::abs(after.red() - 42) +
		std::abs(after.green() - 42) +
		std::abs(after.blue() - 42);

	CHECK(after_delta < before_delta);
}

TEST_CASE("DenoiseImage changes native OpenShot frame format through ProcessFrame", "[effect][denoiseimage][integration]")
{
	std::unique_ptr<EffectBase> effect(EffectInfo().CreateEffect("DenoiseImage"));
	REQUIRE(effect != nullptr);

	auto frame = makeNativeDenoiseFrame();
	REQUIRE(frame->GetImage()->format() == QImage::Format_RGBA8888_Premultiplied);
	const QColor before = frame->GetImage()->pixelColor(2, 2);

	auto out = effect->ProcessFrame(frame, 1);
	const QColor after = out->GetImage()->pixelColor(2, 2);

	CHECK(out->GetImage()->format() == QImage::Format_RGBA8888_Premultiplied);
	CHECK(after != before);
	CHECK(std::abs(after.red() - 42) < std::abs(before.red() - 42));
}

TEST_CASE("DenoiseImage response curve favors shadows", "[effect][denoiseimage][curve]")
{
	DenoiseImage effect;
	effect.strength = Keyframe(1.0);
	effect.detail = Keyframe(0.0);
	effect.temporal = Keyframe(0.0);
	effect.color_noise = Keyframe(1.0);
	effect.response_curve = makeShadowOnlyCurve();

	QImage img(5, 5, QImage::Format_ARGB32);
	img.fill(QColor(40, 40, 40, 255));
	img.setPixelColor(2, 2, QColor(90, 10, 90, 255));
	img.setPixelColor(4, 4, QColor(240, 240, 240, 255));
	img.setPixelColor(3, 3, QColor(255, 210, 255, 255));
	auto frame = std::make_shared<Frame>();
	*frame->GetImage() = img;

	const QColor shadow_before = frame->GetImage()->pixelColor(2, 2);
	const QColor highlight_before = frame->GetImage()->pixelColor(3, 3);
	auto out = effect.GetFrame(frame, 1);
	const QColor shadow_after = out->GetImage()->pixelColor(2, 2);
	const QColor highlight_after = out->GetImage()->pixelColor(3, 3);

	const int shadow_delta = std::abs(shadow_after.red() - shadow_before.red()) +
		std::abs(shadow_after.green() - shadow_before.green()) +
		std::abs(shadow_after.blue() - shadow_before.blue());
	const int highlight_delta = std::abs(highlight_after.red() - highlight_before.red()) +
		std::abs(highlight_after.green() - highlight_before.green()) +
		std::abs(highlight_after.blue() - highlight_before.blue());

	CHECK(shadow_delta > highlight_delta);
}

TEST_CASE("DenoiseImage temporal only applies to sequential frames", "[effect][denoiseimage][temporal]")
{
	DenoiseImage effect;
	effect.strength = Keyframe(1.0);
	effect.detail = Keyframe(0.0);
	effect.temporal = Keyframe(1.0);
	effect.motion_safety = Keyframe(1.0);
	effect.color_noise = Keyframe(1.0);

	auto first = makeFlatFrame(QColor(80, 80, 80, 255));
	effect.GetFrame(first, 1);

	auto sequential = makeFlatFrame(QColor(65, 65, 65, 255));
	auto sequential_out = effect.GetFrame(sequential, 2);
	const QColor sequential_color = sequential_out->GetImage()->pixelColor(2, 2);

	auto jumped = makeFlatFrame(QColor(65, 65, 65, 255));
	auto jumped_out = effect.GetFrame(jumped, 10);
	const QColor jumped_color = jumped_out->GetImage()->pixelColor(2, 2);

	CHECK(sequential_color.red() > jumped_color.red());
}

TEST_CASE("DenoiseImage JSON and PropertiesJSON expose V1 controls", "[effect][denoiseimage][json]")
{
	DenoiseImage effect;
	effect.strength = Keyframe(0.25);
	effect.detail = Keyframe(0.75);
	effect.temporal = Keyframe(0.2);
	effect.motion_safety = Keyframe(0.8);
	effect.color_noise = Keyframe(0.9);
	effect.response_curve = makeShadowOnlyCurve();

	DenoiseImage copy;
	copy.SetJson(effect.Json());

	CHECK(copy.strength.GetValue(1) == Approx(0.25));
	CHECK(copy.detail.GetValue(1) == Approx(0.75));
	CHECK(copy.temporal.GetValue(1) == Approx(0.2));
	CHECK(copy.motion_safety.GetValue(1) == Approx(0.8));
	CHECK(copy.color_noise.GetValue(1) == Approx(0.9));
	CHECK(copy.response_curve.Nodes().size() == 3);

	std::istringstream props(effect.PropertiesJSON(1));
	Json::CharReaderBuilder rb;
	Json::Value root;
	std::string errs;
	REQUIRE(Json::parseFromStream(rb, props, &root, &errs));
	CHECK(root["strength"]["name"].asString() == "Strength");
	CHECK(root["color_noise"]["name"].asString() == "Color Noise");
	CHECK(root["response_curve"]["type"].asString() == "colorgrade_curve");
	CHECK(root["response_curve"].isMember("curve"));
}
