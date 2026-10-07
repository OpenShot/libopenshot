/**
 * @file
 * @brief Unit tests for Blur effect
 *
 * @ref License
 */

// Copyright (c) 2008-2026 OpenShot Studios, LLC
//
// SPDX-License-Identifier: LGPL-3.0-or-later

#include "openshot_catch.h"

#include "Frame.h"
#include "effects/Blur.h"

#include <QColor>

using namespace openshot;

TEST_CASE("Blur margins limit affected area", "[effect][blur]") {
	auto frame = std::make_shared<Frame>(1, 8, 1, "#000000");
	auto image = frame->GetImage();

	for (int x = 0; x < image->width(); ++x)
		image->setPixelColor(x, 0, (x % 2 == 0) ? QColor(255, 255, 255, 255) : QColor(0, 0, 0, 255));

	Blur effect(Keyframe(1.0), Keyframe(0.0), Keyframe(3.0), Keyframe(1.0),
				Keyframe(0.5), Keyframe(0.0), Keyframe(0.0), Keyframe(0.0));
	auto out = effect.GetFrame(frame, 1);
	auto out_image = out->GetImage();

	CHECK(out_image->pixelColor(0, 0) == QColor(255, 255, 255, 255));
	CHECK(out_image->pixelColor(1, 0) == QColor(0, 0, 0, 255));
	CHECK(out_image->pixelColor(2, 0) == QColor(255, 255, 255, 255));
	CHECK(out_image->pixelColor(3, 0) == QColor(0, 0, 0, 255));

	CHECK(out_image->pixelColor(4, 0).red() < 255);
	CHECK(out_image->pixelColor(4, 0).red() > 0);
	CHECK(out_image->pixelColor(5, 0).red() > 0);
}

TEST_CASE("Blur margins handle small affected area", "[effect][blur]") {
	auto frame = std::make_shared<Frame>(1, 8, 8, "#000000");
	auto image = frame->GetImage();

	for (int y = 0; y < image->height(); ++y) {
		for (int x = 0; x < image->width(); ++x) {
			image->setPixelColor(x, y, ((x + y) % 2 == 0)
								 ? QColor(255, 255, 255, 255)
								 : QColor(0, 0, 0, 255));
		}
	}

	Blur effect(Keyframe(6.0), Keyframe(6.0), Keyframe(3.0), Keyframe(1.0),
				Keyframe(0.25), Keyframe(0.25), Keyframe(0.25), Keyframe(0.25));

	REQUIRE_NOTHROW(effect.GetFrame(frame, 1));
	CHECK(image->size() == QSize(8, 8));
}

TEST_CASE("Blur margin properties serialize", "[effect][blur][json]") {
	Blur effect(Keyframe(6.0), Keyframe(6.0), Keyframe(3.0), Keyframe(3.0),
				Keyframe(0.1), Keyframe(0.2), Keyframe(0.3), Keyframe(0.4));

	Json::Value json = effect.JsonValue();
	REQUIRE(json["left"].isObject());
	REQUIRE(json["top"].isObject());
	REQUIRE(json["right"].isObject());
	REQUIRE(json["bottom"].isObject());

	Blur copy;
	copy.SetJsonValue(json);

	CHECK(copy.left.GetValue(1) == Approx(0.1));
	CHECK(copy.top.GetValue(1) == Approx(0.2));
	CHECK(copy.right.GetValue(1) == Approx(0.3));
	CHECK(copy.bottom.GetValue(1) == Approx(0.4));
}
