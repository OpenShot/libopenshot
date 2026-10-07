/**
 * @file
 * @brief Unit tests for FilmGrain effect
 *
 * @ref License
 */

// Copyright (c) 2008-2026 OpenShot Studios, LLC
//
// SPDX-License-Identifier: LGPL-3.0-or-later

#include <QColor>
#include <QImage>
#include <cmath>
#include <memory>
#include <sstream>

#include "Frame.h"
#include "effects/FilmGrain.h"
#include "openshot_catch.h"

using namespace openshot;

static std::shared_ptr<Frame> makeFilmGrainFrame(int w = 32, int h = 32, const QColor& color = QColor(128, 128, 128, 255))
{
	auto frame = std::make_shared<Frame>(1, w, h, "#000000", 0, 2);
	auto img = std::make_shared<QImage>(w, h, QImage::Format_RGBA8888_Premultiplied);
	img->fill(color);
	frame->AddImage(img);
	return frame;
}

static int imageDifference(const QImage& a, const QImage& b)
{
	int total = 0;
	for (int y = 0; y < a.height(); ++y) {
		for (int x = 0; x < a.width(); ++x) {
			const QColor ca = a.pixelColor(x, y);
			const QColor cb = b.pixelColor(x, y);
			total += std::abs(ca.red() - cb.red());
			total += std::abs(ca.green() - cb.green());
			total += std::abs(ca.blue() - cb.blue());
		}
	}
	return total;
}

TEST_CASE("FilmGrain modifies frame", "[effect][filmgrain]")
{
	FilmGrain effect;
	effect.Id("filmgrain-modifies");
	effect.amount = Keyframe(0.8);
	effect.evolution = Keyframe(0.0);
	auto frame = makeFilmGrainFrame();
	const QImage before = frame->GetImage()->copy();
	auto out = effect.GetFrame(frame, 1);
	CHECK(imageDifference(before, *out->GetImage()) > 0);
}

TEST_CASE("FilmGrain deterministic for same id and seed", "[effect][filmgrain]")
{
	FilmGrain e1;
	e1.Id("same");
	e1.seed = 42;
	e1.amount = Keyframe(0.7);
	e1.evolution = Keyframe(0.0);

	FilmGrain e2;
	e2.Id("same");
	e2.seed = 42;
	e2.amount = Keyframe(0.7);
	e2.evolution = Keyframe(0.0);

	auto out1 = e1.GetFrame(makeFilmGrainFrame(), 12);
	auto out2 = e2.GetFrame(makeFilmGrainFrame(), 12);
	CHECK(imageDifference(*out1->GetImage(), *out2->GetImage()) == 0);
}

TEST_CASE("FilmGrain seed alters output", "[effect][filmgrain]")
{
	FilmGrain e1;
	e1.Id("seed");
	e1.seed = 1;
	e1.amount = Keyframe(0.7);
	e1.evolution = Keyframe(0.0);

	FilmGrain e2;
	e2.Id("seed");
	e2.seed = 2;
	e2.amount = Keyframe(0.7);
	e2.evolution = Keyframe(0.0);

	auto out1 = e1.GetFrame(makeFilmGrainFrame(), 1);
	auto out2 = e2.GetFrame(makeFilmGrainFrame(), 1);
	CHECK(imageDifference(*out1->GetImage(), *out2->GetImage()) > 0);
}

TEST_CASE("FilmGrain JSON and properties expose visible controls", "[effect][filmgrain][json]")
{
	FilmGrain effect;
	effect.amount = Keyframe(0.5);
	effect.size = Keyframe(0.4);
	effect.color_amount = Keyframe(0.25);
	effect.seed = 9876;

	FilmGrain copy;
	copy.SetJson(effect.Json());
	CHECK(copy.amount.GetValue(1) == Approx(0.5));
	CHECK(copy.size.GetValue(1) == Approx(0.4));
	CHECK(copy.color_amount.GetValue(1) == Approx(0.25));
	CHECK(copy.seed == 9876);

	std::istringstream props(copy.PropertiesJSON(1));
	Json::CharReaderBuilder rb;
	Json::Value root;
	std::string errs;
	REQUIRE(Json::parseFromStream(rb, props, &root, &errs));
	CHECK(root.isMember("amount"));
	CHECK(root.isMember("size"));
	CHECK(root.isMember("softness"));
	CHECK(root.isMember("clump"));
	CHECK(root.isMember("shadows"));
	CHECK(root.isMember("midtones"));
	CHECK(root.isMember("highlights"));
	CHECK(root.isMember("color_amount"));
	CHECK(root.isMember("color_variation"));
	CHECK(root.isMember("evolution"));
	CHECK(root.isMember("coherence"));
	CHECK(root.isMember("seed"));
	CHECK(root["seed"]["type"].asString() == "int");
}

TEST_CASE("FilmGrain tonal controls weight selected luma range", "[effect][filmgrain]")
{
	FilmGrain effect;
	effect.Id("tonal");
	effect.seed = 7;
	effect.amount = Keyframe(1.0);
	effect.size = Keyframe(0.0);
	effect.softness = Keyframe(0.0);
	effect.clump = Keyframe(0.0);
	effect.evolution = Keyframe(0.0);
	effect.color_amount = Keyframe(0.0);
	effect.shadows = Keyframe(1.0);
	effect.midtones = Keyframe(0.0);
	effect.highlights = Keyframe(0.0);

	auto dark = makeFilmGrainFrame(32, 32, QColor(40, 40, 40, 255));
	auto light = makeFilmGrainFrame(32, 32, QColor(220, 220, 220, 255));
	const QImage dark_before = dark->GetImage()->copy();
	const QImage light_before = light->GetImage()->copy();

	auto dark_out = effect.GetFrame(dark, 1);
	auto light_out = effect.GetFrame(light, 1);
	CHECK(imageDifference(dark_before, *dark_out->GetImage()) > imageDifference(light_before, *light_out->GetImage()));
}

TEST_CASE("FilmGrain amount zero is identity", "[effect][filmgrain]")
{
	FilmGrain effect;
	effect.amount = Keyframe(0.0);
	auto frame = makeFilmGrainFrame();
	const QImage before = frame->GetImage()->copy();
	auto out = effect.GetFrame(frame, 1);
	CHECK(imageDifference(before, *out->GetImage()) == 0);
}

TEST_CASE("FilmGrain structure controls affect grain identity", "[effect][filmgrain]")
{
	FilmGrain baseline;
	baseline.Id("structure");
	baseline.seed = 22;
	baseline.amount = Keyframe(0.9);
	baseline.size = Keyframe(0.0);
	baseline.softness = Keyframe(0.0);
	baseline.clump = Keyframe(0.0);
	baseline.evolution = Keyframe(0.0);

	FilmGrain coarse = baseline;
	coarse.size = Keyframe(1.0);
	FilmGrain soft = baseline;
	soft.softness = Keyframe(1.0);
	FilmGrain clumped = baseline;
	clumped.clump = Keyframe(1.0);

	auto base_out = baseline.GetFrame(makeFilmGrainFrame(), 1);
	CHECK(imageDifference(*base_out->GetImage(), *coarse.GetFrame(makeFilmGrainFrame(), 1)->GetImage()) > 0);
	CHECK(imageDifference(*base_out->GetImage(), *soft.GetFrame(makeFilmGrainFrame(), 1)->GetImage()) > 0);
	CHECK(imageDifference(*base_out->GetImage(), *clumped.GetFrame(makeFilmGrainFrame(), 1)->GetImage()) > 0);
}

TEST_CASE("FilmGrain midtones and highlights can be targeted", "[effect][filmgrain]")
{
	FilmGrain effect;
	effect.Id("tone-targets");
	effect.seed = 8;
	effect.amount = Keyframe(1.0);
	effect.evolution = Keyframe(0.0);
	effect.color_amount = Keyframe(0.0);

	effect.shadows = Keyframe(0.0);
	effect.midtones = Keyframe(1.0);
	effect.highlights = Keyframe(0.0);
	auto mid = makeFilmGrainFrame(32, 32, QColor(128, 128, 128, 255));
	auto light = makeFilmGrainFrame(32, 32, QColor(235, 235, 235, 255));
	const QImage mid_before = mid->GetImage()->copy();
	const QImage light_before = light->GetImage()->copy();
	CHECK(imageDifference(mid_before, *effect.GetFrame(mid, 1)->GetImage()) >
		  imageDifference(light_before, *effect.GetFrame(light, 1)->GetImage()));

	effect.midtones = Keyframe(0.0);
	effect.highlights = Keyframe(1.0);
	auto dark = makeFilmGrainFrame(32, 32, QColor(32, 32, 32, 255));
	light = makeFilmGrainFrame(32, 32, QColor(235, 235, 235, 255));
	const QImage dark_before = dark->GetImage()->copy();
	const QImage highlight_before = light->GetImage()->copy();
	CHECK(imageDifference(highlight_before, *effect.GetFrame(light, 1)->GetImage()) >
		  imageDifference(dark_before, *effect.GetFrame(dark, 1)->GetImage()));
}

TEST_CASE("FilmGrain color controls vary channel correlation", "[effect][filmgrain]")
{
	FilmGrain luma_only;
	luma_only.Id("color");
	luma_only.seed = 55;
	luma_only.amount = Keyframe(1.0);
	luma_only.evolution = Keyframe(0.0);
	luma_only.color_amount = Keyframe(0.0);
	luma_only.color_variation = Keyframe(1.0);

	FilmGrain color_grain = luma_only;
	color_grain.color_amount = Keyframe(1.0);

	auto luma_out = luma_only.GetFrame(makeFilmGrainFrame(), 1);
	auto color_out = color_grain.GetFrame(makeFilmGrainFrame(), 1);
	const QColor luma_pixel = luma_out->GetImage()->pixelColor(3, 3);
	const QColor color_pixel = color_out->GetImage()->pixelColor(3, 3);

	CHECK(luma_pixel.red() == luma_pixel.green());
	CHECK(luma_pixel.green() == luma_pixel.blue());
	CHECK((color_pixel.red() != color_pixel.green() || color_pixel.green() != color_pixel.blue()));
	CHECK(imageDifference(*luma_out->GetImage(), *color_out->GetImage()) > 0);
}

TEST_CASE("FilmGrain temporal controls affect frame-to-frame variation", "[effect][filmgrain]")
{
	FilmGrain static_grain;
	static_grain.Id("temporal");
	static_grain.seed = 9;
	static_grain.amount = Keyframe(0.8);
	static_grain.evolution = Keyframe(0.0);

	FilmGrain evolving = static_grain;
	evolving.evolution = Keyframe(1.0);
	evolving.coherence = Keyframe(0.0);

	auto static_a = static_grain.GetFrame(makeFilmGrainFrame(), 1);
	auto static_b = static_grain.GetFrame(makeFilmGrainFrame(), 30);
	auto evolving_a = evolving.GetFrame(makeFilmGrainFrame(), 1);
	auto evolving_b = evolving.GetFrame(makeFilmGrainFrame(), 30);

	CHECK(imageDifference(*static_a->GetImage(), *static_b->GetImage()) == 0);
	CHECK(imageDifference(*evolving_a->GetImage(), *evolving_b->GetImage()) > 0);
}
