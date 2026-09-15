/**
 * @file
 * @brief Unit tests for Object Mask effect
 * @author Jonathan Thomas <jonathan@openshot.org>
 *
 * @ref License
 */

// Copyright (c) 2026 OpenShot Studios, LLC
//
// SPDX-License-Identifier: LGPL-3.0-or-later

#include "openshot_catch.h"

#include "EffectInfo.h"
#include "Frame.h"
#include "Json.h"
#include "effects/ObjectMask.h"
#ifdef USE_OPENCV
#include "CVObjectMask.h"
#endif

#include <QColor>
#include <QImage>

#include <cstdio>
#include <cstring>
#include <fstream>
#include <memory>

#include <QDir>
#include <QTemporaryFile>

using namespace openshot;

static std::shared_ptr<Frame> make_object_mask_frame(int64_t number, int width, int height) {
	auto frame = std::make_shared<Frame>(number, width, height, "#000000");
	frame->GetImage()->fill(QColor(64, 64, 64, 255));
	return frame;
}

static std::string temp_object_mask_path() {
	QTemporaryFile file(QDir::tempPath() + "/libopenshot_object_mask_XXXXXX.data");
	file.setAutoRemove(false);
	INFO(file.errorString().toStdString());
	REQUIRE(file.open());
	const std::string path = file.fileName().toStdString();
	file.close();
	return path;
}

static void append_varint(std::string& output, uint64_t value) {
	while (value >= 0x80) {
		output.push_back(static_cast<char>((value & 0x7f) | 0x80));
		value >>= 7;
	}
	output.push_back(static_cast<char>(value));
}

static void append_fixed32_float(std::string& output, float value) {
	uint32_t bits;
	std::memcpy(&bits, &value, sizeof(float));
	for (int i = 0; i < 4; ++i)
		output.push_back(static_cast<char>((bits >> (8 * i)) & 0xff));
}

static void append_length_delimited(std::string& output, uint32_t field_number, const std::string& value) {
	append_varint(output, (field_number << 3) | 2);
	append_varint(output, static_cast<uint32_t>(value.size()));
	output.append(value);
}

static std::string create_object_mask_data() {
	const std::string path = temp_object_mask_path();

	std::string mask;
	append_varint(mask, 8);
	append_varint(mask, 4);
	append_varint(mask, 16);
	append_varint(mask, 4);
	for (uint32_t count : {0u, 6u, 10u}) {
		append_varint(mask, 24);
		append_varint(mask, count);
	}

	std::string box;
	append_varint(box, 13);
	append_fixed32_float(box, 0.0f);
	append_varint(box, 21);
	append_fixed32_float(box, 0.0f);
	append_varint(box, 29);
	append_fixed32_float(box, 1.0f);
	append_varint(box, 37);
	append_fixed32_float(box, 1.0f);
	append_varint(box, 40);
	append_varint(box, 0);
	append_varint(box, 53);
	append_fixed32_float(box, 0.95f);
	append_varint(box, 56);
	append_varint(box, 1);
	append_length_delimited(box, 8, mask);

	std::string frame;
	append_varint(frame, 8);
	append_varint(frame, 1);
	append_length_delimited(frame, 2, box);

	std::string data;
	append_length_delimited(data, 1, frame);
	append_length_delimited(data, 3, "object mask");

	std::ofstream output(path, std::ios::out | std::ios::binary);
	output.write(data.data(), static_cast<std::streamsize>(data.size()));
	REQUIRE(output.good());
	return path;
}

TEST_CASE("ObjectMask effect is registered", "[effect][object_mask]") {
	std::unique_ptr<EffectBase> effect(EffectInfo().CreateEffect("ObjectMask"));
	REQUIRE(effect != nullptr);
	CHECK(effect->info.name == "Object Mask");
	CHECK(effect->info.has_video);
	CHECK(effect->info.has_tracked_object);
}

TEST_CASE("ObjectMask loads protobuf masks and exposes style controls", "[effect][object_mask]") {
	const std::string protobuf_path = create_object_mask_data();

	ObjectMask effect;
	Json::Value config;
	config["protobuf_data_path"] = protobuf_path;
	config["mask_alpha"] = Keyframe(0.5).JsonValue();
	config["stroke_width"] = Keyframe(2.0).JsonValue();
	effect.SetJsonValue(config);

	Json::Value properties = stringToJson(effect.PropertiesJSON(1));
	CHECK(properties["draw_mask"]["name"].asString() == "Draw Mask");
	CHECK(properties["mask_color"]["name"].asString() == "Mask Color");
	CHECK(properties["mask_alpha"]["value"].asDouble() == Approx(0.5));
	CHECK(properties["stroke_color"]["name"].asString() == "Stroke Color");
	CHECK(properties["stroke_alpha"]["name"].asString() == "Stroke Alpha");
	CHECK(properties["stroke_width"]["value"].asDouble() == Approx(2.0));

	auto generated_mask = effect.TrackedObjectMask(std::make_shared<QImage>(4, 4, QImage::Format_RGBA8888_Premultiplied), 1);
	REQUIRE(generated_mask != nullptr);
	CHECK(generated_mask->pixelColor(0, 0).red() == 255);
	CHECK(generated_mask->pixelColor(3, 3).red() == 0);

	Json::Value no_stroke;
	no_stroke["stroke_width"] = Keyframe(0.0).JsonValue();
	effect.SetJsonValue(no_stroke);

	auto frame = make_object_mask_frame(1, 4, 4);
	auto output = effect.GetFrame(frame, 1)->GetImage();
	CHECK(output->pixelColor(0, 0) != QColor(64, 64, 64, 255));
	CHECK(output->pixelColor(3, 3) == QColor(64, 64, 64, 255));

	std::remove(protobuf_path.c_str());
}

#ifdef USE_OPENCV
TEST_CASE("CVObjectMask validates a single EfficientSAM ONNX model path", "[effect][object_mask][opencv]") {
	const std::string error = CVObjectMask::ValidateONNXModel("/tmp/libopenshot_missing_efficientsam.onnx");
	CHECK(error.find("Failed to load ONNX model") != std::string::npos);
}
#endif
