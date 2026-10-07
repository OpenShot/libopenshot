/**
 * @file
 * @brief Unit tests for Glow effect
 */

// Copyright (c) 2008-2026 OpenShot Studios, LLC
//
// SPDX-License-Identifier: LGPL-3.0-or-later

#include <memory>

#include <QColor>
#include <QImage>

#include "Frame.h"
#include "effects/Glow.h"
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

static std::shared_ptr<Frame> makeSolidSquareFrame()
{
	QImage img(5, 5, QImage::Format_ARGB32);
	img.fill(QColor(0, 0, 0, 0));
	for (int y = 1; y <= 3; ++y) {
		for (int x = 1; x <= 3; ++x)
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

TEST_CASE("Glow outer mode brightens transparent neighbors", "[effect][glow]")
{
	Glow effect;
	effect.mode = GLOW_MODE_OUTER;
	effect.opacity = Keyframe(1.0);
	effect.blur_radius = Keyframe(1.0);
	effect.spread = Keyframe(0.0);
	effect.color = Color("#ff0000");

	auto frame = makeSinglePixelFrame();
	const QColor before = frame->GetImage()->pixelColor(1, 2);

	auto out = effect.GetFrame(frame, 1);
	const QColor after = out->GetImage()->pixelColor(1, 2);

	CHECK(before.alpha() == 0);
	CHECK(after.alpha() > 0);
}

TEST_CASE("Glow inner mode tints inner edge without affecting outside", "[effect][glow][inner]")
{
	Glow effect;
	effect.mode = GLOW_MODE_INNER;
	effect.opacity = Keyframe(1.0);
	effect.blur_radius = Keyframe(1.0);
	effect.spread = Keyframe(0.0);
	effect.color = Color("#00ff00");

	auto frame = makeSolidSquareFrame();
	const QColor edge_before = frame->GetImage()->pixelColor(1, 2);
	const QColor outside_before = frame->GetImage()->pixelColor(0, 2);

	auto out = effect.GetFrame(frame, 1);
	const QColor edge_after = out->GetImage()->pixelColor(1, 2);
	const QColor outside_after = out->GetImage()->pixelColor(0, 2);

	CHECK(edge_after != edge_before);
	CHECK(outside_before.alpha() == 0);
	CHECK(outside_after.alpha() == 0);
}

TEST_CASE("Glow json round-trip preserves mode and color", "[effect][glow][json]")
{
	Glow effect;
	effect.mode = GLOW_MODE_INNER;
	effect.opacity = Keyframe(0.5);
	effect.blur_radius = Keyframe(9.0);
	effect.spread = Keyframe(0.25);
	effect.color = Color("#204060");

	const Json::Value json = effect.JsonValue();
	CHECK(json["type"].asString() == "Glow");

	Glow copy;
	copy.SetJsonValue(json);
	const Json::Value copy_json = copy.JsonValue();

	CHECK(copy_json["mode"].asInt() == GLOW_MODE_INNER);
	CHECK(copy_json["opacity"]["Points"][0]["co"]["Y"].asDouble() == Approx(0.5));
	CHECK(copy_json["blur_radius"]["Points"][0]["co"]["Y"].asDouble() == Approx(9.0));
	CHECK(copy_json["spread"]["Points"][0]["co"]["Y"].asDouble() == Approx(0.25));
	CHECK(copy_json["color"]["red"]["Points"][0]["co"]["Y"].asDouble() == Approx(32.0));
	CHECK(copy_json["color"]["green"]["Points"][0]["co"]["Y"].asDouble() == Approx(64.0));
	CHECK(copy_json["color"]["blue"]["Points"][0]["co"]["Y"].asDouble() == Approx(96.0));
}

TEST_CASE("Glow spread expands hard-edged shapes before blur", "[effect][glow][spread]")
{
	Glow low;
	low.mode = GLOW_MODE_OUTER;
	low.opacity = Keyframe(1.0);
	low.blur_radius = Keyframe(8.0);
	low.spread = Keyframe(0.0);
	low.color = Color("#ff0000");

	Glow high = low;
	high.spread = Keyframe(1.0);

	auto low_frame = low.GetFrame(makeHardSquareFrame(), 1);
	auto high_frame = high.GetFrame(makeHardSquareFrame(), 1);

	CHECK(high_frame->GetImage()->pixelColor(16, 32).alpha() > low_frame->GetImage()->pixelColor(16, 32).alpha());
}
