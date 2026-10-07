/**
 * @file
 * @brief Unit tests for Mask effect behavior and reader compatibility
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
#include "effects/Mask.h"
#include "QtImageReader.h"
#include "openshot_catch.h"

using namespace openshot;

static std::string temp_png_path(const std::string& base) {
	std::stringstream path;
	path << QDir::tempPath().toStdString() << "/libopenshot_" << base << "_"
		 << getpid() << "_" << rand() << ".png";
	return path.str();
}

static std::string create_mask_png(const std::vector<int>& gray_values) {
	const std::string path = temp_png_path("mask_effect");
	QImage mask(static_cast<int>(gray_values.size()), 1, QImage::Format_RGBA8888_Premultiplied);
	for (size_t i = 0; i < gray_values.size(); ++i) {
		const int gray = gray_values[i];
		mask.setPixelColor(static_cast<int>(i), 0, QColor(gray, gray, gray, 255));
	}
	REQUIRE(mask.save(QString::fromStdString(path)));
	return path;
}

TEST_CASE("Mask applies alpha from reader source", "[effect][mask_effect]") {
	auto frame = std::make_shared<Frame>(1, 2, 1, "#000000");
	auto image = frame->GetImage();
	image->setPixelColor(0, 0, QColor(255, 0, 0, 255));
	image->setPixelColor(1, 0, QColor(255, 0, 0, 255));

	const std::string mask_path = create_mask_png({255, 0});
	Mask mask;
	mask.Reader(new QtImageReader(mask_path));
	mask.brightness = Keyframe(0.0);
	mask.contrast = Keyframe(0.0);

	auto out = mask.GetFrame(frame, 1);

	CHECK(out->GetImage()->pixelColor(0, 0).alpha() == 0);
	CHECK(out->GetImage()->pixelColor(1, 0).alpha() == 255);
}

TEST_CASE("Mask invert flips reader mask alpha mapping", "[effect][mask_effect][invert]") {
	auto frame = std::make_shared<Frame>(1, 2, 1, "#000000");
	auto image = frame->GetImage();
	image->setPixelColor(0, 0, QColor(255, 0, 0, 255));
	image->setPixelColor(1, 0, QColor(255, 0, 0, 255));

	const std::string mask_path = create_mask_png({255, 0});
	Mask mask;
	mask.Reader(new QtImageReader(mask_path));
	mask.mask_invert = true;
	mask.brightness = Keyframe(0.0);
	mask.contrast = Keyframe(0.0);

	auto out = mask.GetFrame(frame, 1);

	CHECK(out->GetImage()->pixelColor(0, 0).alpha() == 255);
	CHECK(out->GetImage()->pixelColor(1, 0).alpha() == 0);
}

TEST_CASE("Mask replace_image emits grayscale values", "[effect][mask_effect][replace]") {
	auto frame = std::make_shared<Frame>(1, 2, 1, "#000000");
	frame->GetImage()->fill(QColor(10, 20, 30, 255));

	const std::string mask_path = create_mask_png({255, 0});
	Mask mask;
	mask.Reader(new QtImageReader(mask_path));
	mask.replace_image = true;
	mask.brightness = Keyframe(0.0);
	mask.contrast = Keyframe(0.0);

	auto out = mask.GetFrame(frame, 1);
	auto px0 = out->GetImage()->pixelColor(0, 0);
	auto px1 = out->GetImage()->pixelColor(1, 0);

	CHECK(px0.red() == px0.green());
	CHECK(px0.green() == px0.blue());
	CHECK(px1.red() == px1.green());
	CHECK(px1.green() == px1.blue());
	CHECK(px0.alpha() == px0.red());
	CHECK(px1.alpha() == px1.red());
}

TEST_CASE("Mask maximum contrast keeps a narrow transition band", "[effect][mask_effect][contrast]") {
	auto frame = std::make_shared<Frame>(1, 3, 1, "#000000");
	auto image = frame->GetImage();
	image->setPixelColor(0, 0, QColor(255, 0, 0, 255));
	image->setPixelColor(1, 0, QColor(255, 0, 0, 255));
	image->setPixelColor(2, 0, QColor(255, 0, 0, 255));

	const std::string mask_path = create_mask_png({128, 129, 134});
	Mask mask;
	mask.Reader(new QtImageReader(mask_path));
	mask.brightness = Keyframe(0.0);
	mask.contrast = Keyframe(20.0);

	auto out = mask.GetFrame(frame, 1);

	CHECK(out->GetImage()->pixelColor(0, 0).alpha() == 127);
	CHECK(out->GetImage()->pixelColor(1, 0).alpha() == 87);
	CHECK(out->GetImage()->pixelColor(2, 0).alpha() == 0);
}

TEST_CASE("Mask accepts legacy reader json field", "[effect][mask_effect][json]") {
	const std::string mask_path = create_mask_png({128});
	QtImageReader reader(mask_path);

	Json::Value root;
	root["reader"] = reader.JsonValue();
	root["brightness"] = Keyframe(0.0).JsonValue();
	root["contrast"] = Keyframe(0.0).JsonValue();

	Mask mask;
	mask.SetJsonValue(root);

	REQUIRE(mask.Reader() != nullptr);
	CHECK(mask.JsonValue().isMember("mask_reader"));
	CHECK(mask.JsonValue()["mask_reader"]["type"].asString() == "QtImageReader");
}

TEST_CASE("Mask legacy start and end json load into base trim", "[effect][mask_effect][json][timing]") {
	Json::Value root;
	root["start"] = 0.5;
	root["end"] = 1.25;
	root["brightness"] = Keyframe(0.0).JsonValue();
	root["contrast"] = Keyframe(0.0).JsonValue();

	Mask mask;
	mask.SetJsonValue(root);

	CHECK(mask.Start() == Approx(0.5).margin(0.00001));
	CHECK(mask.End() == Approx(1.25).margin(0.00001));
	CHECK(mask.JsonValue()["start"].asDouble() == Approx(0.5).margin(0.00001));
	CHECK(mask.JsonValue()["end"].asDouble() == Approx(1.25).margin(0.00001));
}

TEST_CASE("Mask fade_audio_hint json and properties round-trip", "[effect][mask_effect][json][audio]") {
	Mask mask;
	mask.fade_audio_hint = true;

	const Json::Value json = mask.JsonValue();
	CHECK(json["fade_audio_hint"].asBool());

	Mask loaded;
	loaded.SetJsonValue(json);
	CHECK(loaded.fade_audio_hint);

	const Json::Value properties = openshot::stringToJson(loaded.PropertiesJSON(1));
	CHECK(properties["fade_audio_hint"]["value"].asBool());
}

TEST_CASE("Mask fade_audio_hint defaults to false when omitted", "[effect][mask_effect][json][audio][default]") {
	Mask mask;
	mask.SetJsonValue(Json::Value(Json::objectValue));

	CHECK_FALSE(mask.fade_audio_hint);
}

TEST_CASE("Mask ProcessFrame brightness 1.0 fully clears output", "[effect][mask_effect][process][brightness]") {
	auto frame = std::make_shared<Frame>(1, 2, 1, "#000000");
	auto image = frame->GetImage();
	image->setPixelColor(0, 0, QColor(255, 10, 10, 255));
	image->setPixelColor(1, 0, QColor(255, 10, 10, 255));

	const std::string mask_path = create_mask_png({255, 255});
	Mask mask;
	mask.Reader(new QtImageReader(mask_path));
	mask.brightness = Keyframe(1.0);
	mask.contrast = Keyframe(0.0);

	auto out = mask.ProcessFrame(frame, 1);
	CHECK(out->GetImage()->pixelColor(0, 0).alpha() == 0);
	CHECK(out->GetImage()->pixelColor(1, 0).alpha() == 0);
}

TEST_CASE("Mask ProcessFrame honors invert mask property", "[effect][mask_effect][process][invert]") {
	auto frame = std::make_shared<Frame>(1, 2, 1, "#000000");
	auto image = frame->GetImage();
	image->setPixelColor(0, 0, QColor(80, 40, 20, 255));
	image->setPixelColor(1, 0, QColor(80, 40, 20, 255));

	const std::string mask_path = create_mask_png({255, 0});
	Mask mask;
	mask.Reader(new QtImageReader(mask_path));
	mask.mask_invert = true;
	mask.brightness = Keyframe(0.0);
	mask.contrast = Keyframe(0.0);

	auto out = mask.ProcessFrame(frame, 1);
	CHECK(out->GetImage()->pixelColor(0, 0).alpha() == 255);
	CHECK(out->GetImage()->pixelColor(1, 0).alpha() == 0);
}

TEST_CASE("Mask ProcessFrame brightness -1.0 keeps output opaque", "[effect][mask_effect][process][brightness]") {
	auto frame = std::make_shared<Frame>(1, 2, 1, "#000000");
	auto image = frame->GetImage();
	image->setPixelColor(0, 0, QColor(20, 200, 20, 255));
	image->setPixelColor(1, 0, QColor(20, 200, 20, 255));

	const std::string mask_path = create_mask_png({0, 0});
	Mask mask;
	mask.Reader(new QtImageReader(mask_path));
	mask.brightness = Keyframe(-1.0);
	mask.contrast = Keyframe(0.0);

	auto out = mask.ProcessFrame(frame, 1);
	CHECK(out->GetImage()->pixelColor(0, 0).alpha() == 255);
	CHECK(out->GetImage()->pixelColor(1, 0).alpha() == 255);
}

TEST_CASE("Mask ProcessFrame brightness 1.0 ignores gray mask and still clears", "[effect][mask_effect][process][brightness]") {
	auto frame = std::make_shared<Frame>(1, 2, 1, "#000000");
	auto image = frame->GetImage();
	image->setPixelColor(0, 0, QColor(180, 80, 30, 255));
	image->setPixelColor(1, 0, QColor(180, 80, 30, 255));

	const std::string mask_path = create_mask_png({128, 128});
	Mask mask;
	mask.Reader(new QtImageReader(mask_path));
	mask.brightness = Keyframe(1.0);
	mask.contrast = Keyframe(0.0);

	auto out = mask.ProcessFrame(frame, 1);
	CHECK(out->GetImage()->pixelColor(0, 0).alpha() == 0);
	CHECK(out->GetImage()->pixelColor(1, 0).alpha() == 0);
}
