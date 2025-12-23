/**
 * @file
 * @brief Unit tests for AnalogTape effect
 * @author Jonathan Thomas <jonathan@openshot.org>
 *
 * @ref License
 */

// Copyright (c) 2008-2025 OpenShot Studios, LLC
//
// SPDX-License-Identifier: LGPL-3.0-or-later

#include <QColor>
#include <QImage>
#include <memory>

#include "Frame.h"
#include "effects/AnalogTape.h"
#include "openshot_catch.h"

using namespace openshot;

// Fixed helper ensures Frame invariants are respected (size/format/flags)
static std::shared_ptr<Frame> makeGrayFrame(int w = 64, int h = 64) {
	auto f = std::make_shared<Frame>(1, w, h, "#000000", 0, 2);

	// Use premultiplied format to match Frame::AddImage expectations
	auto img = std::make_shared<QImage>(w, h, QImage::Format_RGBA8888_Premultiplied);
	img->fill(QColor(100, 100, 100, 255));

	// Route through AddImage so width/height/has_image_data are set correctly
	f->AddImage(img);
	return f;
}

TEST_CASE("AnalogTape modifies frame", "[effect][analogtape]") {
	AnalogTape eff;
	eff.Id("analogtape-test-seed");
	eff.seed_offset = 1234;
	auto frame = makeGrayFrame();
	QColor before = frame->GetImage()->pixelColor(2, 2);
	auto out = eff.GetFrame(frame, 1);
	QColor after = out->GetImage()->pixelColor(2, 2);
	CHECK(after != before);
}

TEST_CASE("AnalogTape deterministic per id", "[effect][analogtape]") {
	AnalogTape e1;
	e1.Id("same");
	AnalogTape e2;
	e2.Id("same");
	auto f1 = makeGrayFrame();
	auto f2 = makeGrayFrame();
	auto o1 = e1.GetFrame(f1, 1);
	auto o2 = e2.GetFrame(f2, 1);
	QColor c1 = o1->GetImage()->pixelColor(1, 1);
	QColor c2 = o2->GetImage()->pixelColor(1, 1);
	CHECK(c1 == c2);
}

TEST_CASE("AnalogTape seed offset alters output", "[effect][analogtape]") {
	AnalogTape e1;
	e1.Id("seed");
	e1.seed_offset = 0;
	AnalogTape e2;
	e2.Id("seed");
	e2.seed_offset = 5;
	auto f1 = makeGrayFrame();
	auto f2 = makeGrayFrame();
	auto o1 = e1.GetFrame(f1, 1);
	auto o2 = e2.GetFrame(f2, 1);
	QColor c1 = o1->GetImage()->pixelColor(1, 1);
	QColor c2 = o2->GetImage()->pixelColor(1, 1);
	CHECK(c1 != c2);
}

TEST_CASE("AnalogTape stripe lifts bottom", "[effect][analogtape]") {
	AnalogTape e;
	e.tracking = Keyframe(0.0);
	e.bleed = Keyframe(0.0);
	e.softness = Keyframe(0.0);
	e.noise = Keyframe(0.0);
	e.stripe = Keyframe(1.0);
	e.staticBands = Keyframe(0.0);
	auto frame = makeGrayFrame(20, 20);
	auto out = e.GetFrame(frame, 1);
	QColor top = out->GetImage()->pixelColor(10, 0);
	QColor bottom = out->GetImage()->pixelColor(10, 19);
	CHECK(bottom.red() > top.red());
}
