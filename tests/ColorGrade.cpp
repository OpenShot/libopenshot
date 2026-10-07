/**
 * @file
 * @brief Unit tests for ColorGrade effect
 *
 * @ref License
 */

// Copyright (c) 2008-2026 OpenShot Studios, LLC
//
// SPDX-License-Identifier: LGPL-3.0-or-later

#include <memory>
#include <QColor>
#include <QImage>
#include <sstream>

#include "Frame.h"
#include "effects/ColorGrade.h"
#include "openshot_catch.h"

using namespace openshot;

static std::shared_ptr<Frame> makeGradeFrame()
{
	QImage img(2, 2, QImage::Format_ARGB32);
	img.fill(QColor(80, 90, 110, 255));
	img.setPixelColor(0, 0, QColor(64, 96, 160, 255));
	auto frame = std::make_shared<Frame>();
	*frame->GetImage() = img;
	return frame;
}

static std::shared_ptr<Frame> makeTransparentGradeFrame()
{
	QImage img(2, 2, QImage::Format_ARGB32_Premultiplied);
	img.fill(QColor(40, 50, 70, 128));
	img.setPixelColor(0, 0, QColor(64, 96, 128, 128));
	auto frame = std::make_shared<Frame>();
	*frame->GetImage() = img;
	return frame;
}

static std::string lutPath()
{
	std::stringstream path;
	path << TEST_MEDIA_PATH << "example-lut.cube";
	return path.str();
}

static AnimatedCurve makeBrightCurve()
{
	AnimatedCurve curve;
	curve.Nodes().clear();

	AnimatedCurveNode left(0, 0.0, 0.0, LINEAR);
	AnimatedCurveNode middle(1, 0.5, 0.8, LINEAR);
	AnimatedCurveNode right(2, 1.0, 1.0, LINEAR);

	curve.Nodes().push_back(left);
	curve.Nodes().push_back(middle);
	curve.Nodes().push_back(right);
	return curve;
}

TEST_CASE("Default ColorGrade leaves image unchanged", "[effect][colorgrade]")
{
	ColorGrade effect;
	auto frame = makeGradeFrame();
	const QColor before = frame->GetImage()->pixelColor(0, 0);
	auto out = effect.GetFrame(frame, 1);
	const QColor after = out->GetImage()->pixelColor(0, 0);
	CHECK(after == before);
}

TEST_CASE("ColorGrade JSON round-trip preserves animated wheels and curves", "[effect][colorgrade][json]")
{
	ColorGrade effect;
	effect.temperature = Keyframe(0.25);
	effect.mix = Keyframe(0.75);
	effect.wheels.enabled = Keyframe(0.0);
	effect.wheels.global.color = Color("#ff8800");
	effect.wheels.global.amount = Keyframe(0.4);
	effect.wheels.global.luma = Keyframe(0.1);
	effect.curve_all = makeBrightCurve();
	effect.curve_all.enabled = Keyframe(0.0);
	effect.curve_all.Nodes()[1].y.AddPoint(12, 0.9, LINEAR);

	ColorGrade copy;
	copy.SetJson(effect.Json());

	CHECK(copy.temperature.GetValue(1) == Approx(0.25));
	CHECK(copy.mix.GetValue(1) == Approx(0.75));
	CHECK(copy.wheels.enabled.GetValue(1) == Approx(0.0));
	CHECK(copy.JsonValue()["wheels"]["global"]["color"].asString() == "#ff8800");
	CHECK(copy.JsonValue()["curve_all"]["nodes"].size() == 3);
	CHECK(copy.curve_all.Nodes()[1].y.GetValue(12) == Approx(0.9));
	CHECK(copy.curve_all.enabled.GetValue(1) == Approx(0.0));
}

TEST_CASE("PropertiesJSON exposes animated wheels and curve properties", "[effect][colorgrade][ui]")
{
	ColorGrade effect;
	std::istringstream props(effect.PropertiesJSON(1));
	Json::CharReaderBuilder rb;
	Json::Value root;
	std::string errs;
	REQUIRE(Json::parseFromStream(rb, props, &root, &errs));
	CHECK(root.isMember("wheels"));
	CHECK(root["wheels"]["type"].asString() == "colorgrade_wheels");
	CHECK(root["wheels"].isMember("wheels"));
	CHECK(root["wheels"]["wheels"]["enabled"].asBool() == true);
	CHECK(root.isMember("curve_all"));
	CHECK(root["curve_all"]["type"].asString() == "colorgrade_curve");
	CHECK(root["curve_all"].isMember("curve"));
	CHECK(root["curve_all"]["curve"]["enabled"].isObject());
	CHECK(root["curve_all"]["curve"]["nodes"].size() == 2);
}

TEST_CASE("Animated curve data brightens matching pixels", "[effect][colorgrade][curve]")
{
	ColorGrade effect;
	effect.curve_all = makeBrightCurve();

	auto frame = makeGradeFrame();
	const QColor before = frame->GetImage()->pixelColor(0, 0);
	auto out = effect.GetFrame(frame, 1);
	const QColor after = out->GetImage()->pixelColor(0, 0);
	CHECK(after.red() > before.red());
}

TEST_CASE("Animated curve data supports smooth bezier points", "[effect][colorgrade][curve][bezier]")
{
	ColorGrade effect;
	effect.curve_all.Nodes().clear();

	AnimatedCurveNode left(0, 0.0, 0.0, BEZIER);
	left.right_handle_x = Keyframe(0.2);
	left.right_handle_y = Keyframe(0.0);

	AnimatedCurveNode middle(1, 0.5, 0.8, BEZIER);
	middle.left_handle_x = Keyframe(0.8);
	middle.left_handle_y = Keyframe(1.0);
	middle.right_handle_x = Keyframe(0.2);
	middle.right_handle_y = Keyframe(0.0);

	AnimatedCurveNode right(2, 1.0, 1.0, BEZIER);
	right.left_handle_x = Keyframe(0.8);
	right.left_handle_y = Keyframe(1.0);

	effect.curve_all.Nodes().push_back(left);
	effect.curve_all.Nodes().push_back(middle);
	effect.curve_all.Nodes().push_back(right);

	auto frame = makeGradeFrame();
	const QColor before = frame->GetImage()->pixelColor(0, 0);
	auto out = effect.GetFrame(frame, 1);
	const QColor after = out->GetImage()->pixelColor(0, 0);
	CHECK(after.red() > before.red());
}

TEST_CASE("Animated curve nodes interpolate over time", "[effect][colorgrade][curve][animation]")
{
	ColorGrade effect;
	effect.curve_all.Nodes().clear();

	AnimatedCurveNode left(10, 0.0, 0.0, LINEAR);
	left.y.AddPoint(300, 0.5, LINEAR);
	AnimatedCurveNode right(20, 1.0, 1.0, LINEAR);
	right.y.AddPoint(300, 0.75, LINEAR);

	effect.curve_all.Nodes().push_back(left);
	effect.curve_all.Nodes().push_back(right);

	CHECK(effect.curve_all.Sample(0.0f, 1) == Approx(0.0f).margin(0.01f));
	CHECK(effect.curve_all.Sample(0.0f, 300) == Approx(0.5f).margin(0.01f));
	CHECK(effect.curve_all.Sample(1.0f, 1) == Approx(1.0f).margin(0.01f));
	CHECK(effect.curve_all.Sample(1.0f, 300) == Approx(0.75f).margin(0.01f));

	auto frame_a = makeGradeFrame();
	auto frame_b = makeGradeFrame();
	auto frame_mid = makeGradeFrame();
	const int before = frame_a->GetImage()->pixelColor(0, 0).red();
	const int after_a = effect.GetFrame(frame_a, 1)->GetImage()->pixelColor(0, 0).red();
	const int after_b = effect.GetFrame(frame_b, 300)->GetImage()->pixelColor(0, 0).red();
	const int after_mid = effect.GetFrame(frame_mid, 150)->GetImage()->pixelColor(0, 0).red();

	CHECK(after_b != before);
	CHECK(after_mid != after_a);
	CHECK(after_mid != after_b);
}

TEST_CASE("Disabled animated curve renders as identity", "[effect][colorgrade][curve]")
{
	ColorGrade effect;
	effect.curve_all = makeBrightCurve();
	effect.curve_all.enabled = Keyframe(0.0);

	auto frame = makeGradeFrame();
	const QColor before = frame->GetImage()->pixelColor(0, 0);
	auto out = effect.GetFrame(frame, 1);
	const QColor after = out->GetImage()->pixelColor(0, 0);

	CHECK(after == before);
	CHECK(effect.curve_all.JsonValue()["nodes"].size() == 3);
}

TEST_CASE("Wheel data shifts tonal balance", "[effect][colorgrade][wheels]")
{
	ColorGrade effect;
	effect.wheels.shadows.color = Color("#ff6600");
	effect.wheels.shadows.amount = Keyframe(0.8);
	effect.wheels.shadows.luma = Keyframe(0.1);

	auto frame = makeGradeFrame();
	const QColor before = frame->GetImage()->pixelColor(0, 0);
	auto out = effect.GetFrame(frame, 1);
	const QColor after = out->GetImage()->pixelColor(0, 0);
	CHECK(after != before);
}

TEST_CASE("Wheel data interpolates over time with native keyframes", "[effect][colorgrade][wheels][animation]")
{
	ColorGrade effect;
	effect.wheels.enabled = Keyframe(1.0);
	effect.wheels.global.color.red = Keyframe(255.0);
	effect.wheels.global.color.green = Keyframe(255.0);
	effect.wheels.global.color.green.AddPoint(11, 0, LINEAR);
	effect.wheels.global.color.blue = Keyframe(255.0);
	effect.wheels.global.color.blue.AddPoint(11, 0, LINEAR);
	effect.wheels.global.color.alpha = Keyframe(255.0);
	effect.wheels.global.amount = Keyframe(0.0);
	effect.wheels.global.luma = Keyframe(0.0);
	effect.wheels.global.luma.AddPoint(11, 0.3, LINEAR);

	const QColor wheel_a = effect.wheels.global.GetColor(1);
	const QColor wheel_mid = effect.wheels.global.GetColor(6);
	const QColor wheel_b = effect.wheels.global.GetColor(11);
	CHECK(wheel_a.green() == 255);
	CHECK(wheel_b.green() == 0);
	CHECK(wheel_mid.green() < wheel_a.green());
	CHECK(wheel_mid.green() > wheel_b.green());

	auto frame_a = makeGradeFrame();
	auto frame_b = makeGradeFrame();
	auto frame_mid = makeGradeFrame();
	const int before = frame_a->GetImage()->pixelColor(0, 0).red();
	const int after_a = effect.GetFrame(frame_a, 1)->GetImage()->pixelColor(0, 0).red();
	const int after_b = effect.GetFrame(frame_b, 11)->GetImage()->pixelColor(0, 0).red();
	const int after_mid = effect.GetFrame(frame_mid, 6)->GetImage()->pixelColor(0, 0).red();

	CHECK(after_a == before);
	CHECK(after_b > before);
	CHECK(after_mid > after_a);
	CHECK(after_mid < after_b);
}

TEST_CASE("Disabled wheels preserve values but render as identity", "[effect][colorgrade][wheels]")
{
	ColorGrade effect;
	effect.wheels.enabled = Keyframe(0.0);
	effect.wheels.shadows.color = Color("#ff6600");
	effect.wheels.shadows.amount = Keyframe(0.8);
	effect.wheels.shadows.luma = Keyframe(0.1);

	auto frame = makeGradeFrame();
	const QColor before = frame->GetImage()->pixelColor(0, 0);
	auto out = effect.GetFrame(frame, 1);
	const QColor after = out->GetImage()->pixelColor(0, 0);

	CHECK(after == before);
	CHECK(effect.wheels.enabled.GetValue(1) == Approx(0.0));
	CHECK(effect.wheels.shadows.amount.GetValue(1) == Approx(0.8));
}

TEST_CASE("Identity ColorGrade preserves semi-transparent pixels", "[effect][colorgrade][alpha]")
{
	ColorGrade effect;
	auto frame = makeTransparentGradeFrame();
	const QColor before = frame->GetImage()->pixelColor(0, 0);
	auto out = effect.GetFrame(frame, 1);
	const QColor after = out->GetImage()->pixelColor(0, 0);

	CHECK(after == before);
}

TEST_CASE("LUT path and intensity reuse ColorMap behavior", "[effect][colorgrade][lut]")
{
	ColorGrade effect;
	Json::Value update(Json::objectValue);
	update["lut_path"] = lutPath();
	update["lut_intensity"] = Keyframe(1.0).JsonValue();
	effect.SetJsonValue(update);

	auto frame = makeGradeFrame();
	const QColor before = frame->GetImage()->pixelColor(0, 0);
	auto out = effect.GetFrame(frame, 1);
	const QColor after = out->GetImage()->pixelColor(0, 0);
	CHECK(after != before);
}
