/**
 * @file
 * @brief Unit tests for Displace effect behavior and reader separation
 */

// Copyright (c) 2008-2026 OpenShot Studios, LLC
//
// SPDX-License-Identifier: LGPL-3.0-or-later

#include <cstdlib>
#include <memory>
#include <sstream>
#include <string>
#include <vector>
#include <unistd.h>

#include <QColor>
#include <QDir>
#include <QImage>

#include "Frame.h"
#include "effects/Displace.h"
#include "QtImageReader.h"
#include "openshot_catch.h"

using namespace openshot;

static std::string temp_png_path(const std::string& base) {
	std::stringstream path;
	path << QDir::tempPath().toStdString() << "/libopenshot_" << base << "_"
		 << getpid() << "_" << rand() << ".png";
	return path.str();
}

static std::string create_map_png(const std::vector<int>& gray_values) {
	const std::string path = temp_png_path("displace_map");
	QImage map(static_cast<int>(gray_values.size()), 1, QImage::Format_RGBA8888_Premultiplied);
	for (size_t i = 0; i < gray_values.size(); ++i) {
		const int gray = gray_values[i];
		map.setPixelColor(static_cast<int>(i), 0, QColor(gray, gray, gray, 255));
	}
	REQUIRE(map.save(QString::fromStdString(path)));
	return path;
}

static std::string create_mask_png(const std::vector<int>& gray_values) {
	const std::string path = temp_png_path("displace_mask");
	QImage mask(static_cast<int>(gray_values.size()), 1, QImage::Format_RGBA8888_Premultiplied);
	for (size_t i = 0; i < gray_values.size(); ++i) {
		const int gray = gray_values[i];
		mask.setPixelColor(static_cast<int>(i), 0, QColor(gray, gray, gray, 255));
	}
	REQUIRE(mask.save(QString::fromStdString(path)));
	return path;
}

TEST_CASE("Displace serializes map_reader independently from shared mask_reader", "[effect][displace][json]") {
	const std::string map_path = create_map_png({255, 255, 255});
	const std::string mask_path = create_mask_png({255, 0, 0});

	Displace effect;
	effect.Reader(new QtImageReader(map_path));
	effect.MaskReader(new QtImageReader(mask_path));
	effect.invert = true;
	effect.mask_invert = true;

	const Json::Value json = effect.JsonValue();
	REQUIRE(json.isMember("map_reader"));
	REQUIRE(json.isMember("mask_reader"));
	CHECK(json["map_reader"]["type"].asString() == "QtImageReader");
	CHECK(json["mask_reader"]["type"].asString() == "QtImageReader");
	CHECK(json["map_reader"]["path"].asString() == map_path);
	CHECK(json["mask_reader"]["path"].asString() == mask_path);

	Displace copy;
	copy.SetJsonValue(json);

	REQUIRE(copy.Reader() != nullptr);
	REQUIRE(copy.MaskReader() != nullptr);
	CHECK(copy.JsonValue()["map_reader"]["path"].asString() == map_path);
	CHECK(copy.JsonValue()["mask_reader"]["path"].asString() == mask_path);
	CHECK(copy.invert);
	CHECK(copy.mask_invert);
}

TEST_CASE("Displace ProcessFrame uses map source and shared mask together", "[effect][displace][process][mask]") {
	auto frame = std::make_shared<Frame>(1, 3, 1, "#000000");
	auto image = frame->GetImage();
	image->setPixelColor(0, 0, QColor(255, 0, 0, 255));
	image->setPixelColor(1, 0, QColor(0, 255, 0, 255));
	image->setPixelColor(2, 0, QColor(0, 0, 255, 255));

	const std::string map_path = create_map_png({255, 255, 255});
	const std::string mask_path = create_mask_png({255, 0, 0});

	Displace effect;
	effect.Reader(new QtImageReader(map_path));
	effect.MaskReader(new QtImageReader(mask_path));
	effect.strength = Keyframe(1.0);
	effect.horizontal = Keyframe(1.0);
	effect.vertical = Keyframe(0.0);
	effect.brightness = Keyframe(0.0);
	effect.contrast = Keyframe(0.0);

	auto out = effect.ProcessFrame(frame, 1);
	auto out_image = out->GetImage();

	// The displacement map pushes sampling to the right edge, so masked pixels
	// become blue while unmasked pixels keep their original colors.
	CHECK(out_image->pixelColor(0, 0).red() == 0);
	CHECK(out_image->pixelColor(0, 0).green() == 0);
	CHECK(out_image->pixelColor(0, 0).blue() == 255);

	CHECK(out_image->pixelColor(1, 0).red() == 0);
	CHECK(out_image->pixelColor(1, 0).green() == 255);
	CHECK(out_image->pixelColor(1, 0).blue() == 0);

	CHECK(out_image->pixelColor(2, 0).red() == 0);
	CHECK(out_image->pixelColor(2, 0).green() == 0);
	CHECK(out_image->pixelColor(2, 0).blue() == 255);
}
