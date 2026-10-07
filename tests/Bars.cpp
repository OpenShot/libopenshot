/**
 * @file
 * @brief Unit tests for Bars effect
 *
 * @ref License
 */

// Copyright (c) 2008-2026 OpenShot Studios, LLC
//
// SPDX-License-Identifier: LGPL-3.0-or-later

#include "openshot_catch.h"

#include "Frame.h"
#include "effects/Bars.h"

#include <QColor>
#include <QSize>

using namespace openshot;

TEST_CASE("Bars margins clamp out-of-range values", "[effect][bars]") {
	auto frame = std::make_shared<Frame>(1, 8, 8, "#336699");
	auto image = frame->GetImage();

	Bars effect(Color("#000000"), Keyframe(2.0), Keyframe(-1.0), Keyframe(2.0), Keyframe(-1.0));

	REQUIRE_NOTHROW(effect.GetFrame(frame, 1));
	CHECK(image->size() == QSize(8, 8));
}

TEST_CASE("Bars margins handle full-frame bars", "[effect][bars]") {
	auto frame = std::make_shared<Frame>(1, 8, 8, "#336699");
	auto image = frame->GetImage();

	Bars effect(Color("#000000"), Keyframe(0.0), Keyframe(1.0), Keyframe(0.0), Keyframe(1.0));

	REQUIRE_NOTHROW(effect.GetFrame(frame, 1));
	CHECK(image->pixelColor(0, 0) == QColor(0, 0, 0, 255));
	CHECK(image->pixelColor(7, 7) == QColor(0, 0, 0, 255));
}
