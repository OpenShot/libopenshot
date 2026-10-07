/**
 * @file
 * @brief Unit tests for Shadow effect
 */

// Copyright (c) 2008-2026 OpenShot Studios, LLC
//
// SPDX-License-Identifier: LGPL-3.0-or-later

#include <memory>

#include <QColor>
#include <QImage>

#include "Frame.h"
#include "effects/Shadow.h"
#include "openshot_catch.h"

using namespace openshot;

static std::ostream& operator<<(std::ostream& os, QColor const& c)
{
	os << "QColor(" << c.red() << "," << c.green()
	   << "," << c.blue() << "," << c.alpha() << ")";
	return os;
}

static std::shared_ptr<Frame> makeSinglePixelFrame()
{
	QImage img(5, 5, QImage::Format_ARGB32);
	img.fill(QColor(0, 0, 0, 0));
	img.setPixelColor(2, 2, QColor(255, 255, 255, 255));
	auto frame = std::make_shared<Frame>();
	*frame->GetImage() = img;
	return frame;
}

static std::shared_ptr<Frame> makeTopEdgeFrame()
{
	QImage img(64, 64, QImage::Format_ARGB32);
	img.fill(QColor(0, 0, 0, 0));
	for (int y = 0; y < 24; ++y) {
		for (int x = 20; x < 44; ++x)
			img.setPixelColor(x, y, QColor(255, 255, 255, 255));
	}
	auto frame = std::make_shared<Frame>();
	*frame->GetImage() = img;
	return frame;
}

static std::shared_ptr<Frame> makeHardSquareFrame()
{
	QImage img(64, 64, QImage::Format_ARGB32);
	img.fill(QColor(0, 0, 0, 0));
	for (int y = 20; y < 44; ++y) {
		for (int x = 20; x < 44; ++x)
			img.setPixelColor(x, y, QColor(255, 255, 255, 255));
	}
	auto frame = std::make_shared<Frame>();
	*frame->GetImage() = img;
	return frame;
}

TEST_CASE("Shadow offsets visible pixels into surrounding area", "[effect][shadow]")
{
	Shadow effect;
	effect.opacity = Keyframe(1.0);
	effect.blur_radius = Keyframe(0.0);
	effect.spread = Keyframe(0.0);
	effect.distance = Keyframe(1.0);
	effect.angle = Keyframe(0.0);
	effect.color = Color("#000000");

	auto frame = makeSinglePixelFrame();
	const QColor before = frame->GetImage()->pixelColor(3, 2);

	auto out = effect.GetFrame(frame, 1);
	const QColor after = out->GetImage()->pixelColor(3, 2);

	CHECK(before.alpha() == 0);
	CHECK(after.alpha() > 0);
}

TEST_CASE("Shadow blur can still reach the canvas edge after offset", "[effect][shadow][edge]")
{
	Shadow effect;
	effect.opacity = Keyframe(1.0);
	effect.blur_radius = Keyframe(12.0);
	effect.spread = Keyframe(0.0);
	effect.distance = Keyframe(10.0);
	effect.angle = Keyframe(90.0);
	effect.color = Color("#000000");

	auto frame = makeTopEdgeFrame();
	auto out = effect.GetFrame(frame, 1);

	CHECK(out->GetImage()->pixelColor(32, 0).alpha() > 0);
}

TEST_CASE("Shadow spread expands hard-edged shapes before blur", "[effect][shadow][spread]")
{
	Shadow low;
	low.opacity = Keyframe(1.0);
	low.blur_radius = Keyframe(8.0);
	low.spread = Keyframe(0.0);
	low.distance = Keyframe(0.0);
	low.angle = Keyframe(0.0);
	low.color = Color("#000000");

	Shadow high = low;
	high.spread = Keyframe(1.0);

	auto low_frame = low.GetFrame(makeHardSquareFrame(), 1);
	auto high_frame = high.GetFrame(makeHardSquareFrame(), 1);

	CHECK(high_frame->GetImage()->pixelColor(16, 32).alpha() > low_frame->GetImage()->pixelColor(16, 32).alpha());
}

TEST_CASE("Shadow json round-trip preserves key properties", "[effect][shadow][json]")
{
	Shadow effect;
	effect.opacity = Keyframe(0.42);
	effect.blur_radius = Keyframe(7.0);
	effect.spread = Keyframe(0.33);
	effect.distance = Keyframe(11.0);
	effect.angle = Keyframe(60.0);
	effect.color = Color("#102030");

	const Json::Value json = effect.JsonValue();
	CHECK(json["type"].asString() == "Shadow");

	Shadow copy;
	copy.SetJsonValue(json);
	const Json::Value copy_json = copy.JsonValue();

	CHECK(copy_json["opacity"]["Points"][0]["co"]["Y"].asDouble() == Approx(0.42));
	CHECK(copy_json["blur_radius"]["Points"][0]["co"]["Y"].asDouble() == Approx(7.0));
	CHECK(copy_json["spread"]["Points"][0]["co"]["Y"].asDouble() == Approx(0.33));
	CHECK(copy_json["distance"]["Points"][0]["co"]["Y"].asDouble() == Approx(11.0));
	CHECK(copy_json["angle"]["Points"][0]["co"]["Y"].asDouble() == Approx(60.0));
	CHECK(copy_json["color"]["red"]["Points"][0]["co"]["Y"].asDouble() == Approx(16.0));
	CHECK(copy_json["color"]["green"]["Points"][0]["co"]["Y"].asDouble() == Approx(32.0));
	CHECK(copy_json["color"]["blue"]["Points"][0]["co"]["Y"].asDouble() == Approx(48.0));
}
