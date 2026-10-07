/**
 * @file
 * @brief Unit tests for Pixelate effect
 *
 * @ref License
 */

// Copyright (c) 2008-2026 OpenShot Studios, LLC
//
// SPDX-License-Identifier: LGPL-3.0-or-later

#include "openshot_catch.h"

#include "Frame.h"
#include "effects/Pixelate.h"

#include <QSize>

using namespace openshot;

TEST_CASE("Pixelate margins handle collapsed area", "[effect][pixelate]") {
	auto frame = std::make_shared<Frame>(1, 8, 8, "#336699");
	auto image = frame->GetImage();

	Pixelate effect(Keyframe(0.5), Keyframe(0.75), Keyframe(0.0), Keyframe(0.75), Keyframe(0.0));

	REQUIRE_NOTHROW(effect.GetFrame(frame, 1));
	CHECK(image->size() == QSize(8, 8));
}

TEST_CASE("Pixelate margins clamp out-of-range values", "[effect][pixelate]") {
	auto frame = std::make_shared<Frame>(1, 8, 8, "#336699");
	auto image = frame->GetImage();

	Pixelate effect(Keyframe(0.5), Keyframe(-1.0), Keyframe(-1.0), Keyframe(2.0), Keyframe(2.0));

	REQUIRE_NOTHROW(effect.GetFrame(frame, 1));
	CHECK(image->size() == QSize(8, 8));
}
