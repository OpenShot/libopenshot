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

#include "Clip.h"
#include "EffectInfo.h"
#include "Frame.h"
#include "Json.h"
#include "effects/ObjectMask.h"
#ifdef USE_OPENCV
#include "CVObjectMask.h"
#include "objdetectdata.pb.h"
#endif

#include <QColor>
#include <QFile>
#include <QImage>
#include <QTemporaryDir>

#include <cstring>
#include <memory>

using namespace openshot;

static std::shared_ptr<Frame> make_object_mask_frame(int64_t number, int width, int height) {
	auto frame = std::make_shared<Frame>(number, width, height, "#000000");
	frame->GetImage()->fill(QColor(64, 64, 64, 255));
	return frame;
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

static void create_object_mask_data(const QString& path) {
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

	QFile output(path);
	REQUIRE(output.open(QIODevice::WriteOnly));
	REQUIRE(output.write(data.data(), static_cast<qint64>(data.size())) == static_cast<qint64>(data.size()));
	REQUIRE(output.flush());
}

TEST_CASE("ObjectMask effect is registered", "[effect][object_mask]") {
	std::unique_ptr<EffectBase> effect(EffectInfo().CreateEffect("ObjectMask"));
	REQUIRE(effect != nullptr);
	CHECK(effect->info.name == "Object Mask");
	CHECK(effect->info.has_video);
	CHECK(effect->info.has_tracked_object);
}

TEST_CASE("ObjectMask loads protobuf masks and exposes style controls", "[effect][object_mask]") {
	QTemporaryDir directory;
	REQUIRE(directory.isValid());
	const QString protobuf_path = directory.filePath("object_mask.data");
	create_object_mask_data(protobuf_path);

	ObjectMask effect;
	Json::Value config;
	config["protobuf_data_path"] = protobuf_path.toUtf8().toStdString();
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
}


#ifdef USE_OPENCV
TEST_CASE("CVObjectMask handles empty preview and mask structures",
          "[effect][object_mask][opencv]") {
    CVObjectMaskFrameData frameData;
    CHECK_FALSE(frameData.HasMask());

    frameData.width = 4;
    frameData.height = 4;
    CHECK_FALSE(frameData.HasMask());

    frameData.rle = {0, 6, 10};
    CHECK(frameData.HasMask());

    CVObjectMaskPromptSet prompts;
    CHECK_FALSE(prompts.HasPositivePrompt());

    prompts.negativePoints.emplace_back(1.0f, 2.0f);
    CHECK_FALSE(prompts.HasPositivePrompt());

    prompts.positivePoints.emplace_back(2.0f, 3.0f);
    CHECK(prompts.HasPositivePrompt());

    ProcessingController controller;
    CVObjectMask processor("{}", controller);

    CHECK(processor.PreviewSeedMask(nullptr) == nullptr);

    auto frame = make_object_mask_frame(1, 4, 4);
    CHECK(processor.PreviewSeedMask(frame) == nullptr);
}

TEST_CASE("CVObjectMask saves synthetic protobuf masks",
          "[effect][object_mask][opencv]") {
    QTemporaryDir directory;
    REQUIRE(directory.isValid());

    const QString outputPath = directory.filePath("generated_mask.data");

    ProcessingController controller;
    CVObjectMask processor("{}", controller);

    CHECK_FALSE(processor.SaveObjMaskData());

    Json::Value config;
    config["protobuf_data_path"] = outputPath.toStdString();
    processor.SetJsonValue(config);

    CVObjectMaskFrameData emptyFrame;
    emptyFrame.frameId = 1;
    processor.masksData[1] = emptyFrame;

    CVObjectMaskFrameData maskFrame;
    maskFrame.frameId = 2;
    maskFrame.box = cv::Rect2f(0.0f, 0.0f, 1.0f, 1.0f);
    maskFrame.score = 0.95f;
    maskFrame.objectId = 3;
    maskFrame.width = 4;
    maskFrame.height = 4;
    maskFrame.rle = {0, 6, 10};
    processor.masksData[2] = maskFrame;

    REQUIRE(processor.SaveObjMaskData());

    QFile output(outputPath);
    REQUIRE(output.open(QIODevice::ReadOnly));

    const QByteArray serialized = output.readAll();
    REQUIRE_FALSE(serialized.isEmpty());

    pb_objdetect::ObjDetect decoded;
    REQUIRE(decoded.ParseFromArray(
        serialized.constData(),
        static_cast<int>(serialized.size())));

    REQUIRE(decoded.classnames_size() == 1);
    CHECK(decoded.classnames(0) == "object mask");

    REQUIRE(decoded.frame_size() == 2);

    const auto& first = decoded.frame(0);
    CHECK(first.id() == 1);
    CHECK(first.bounding_box_size() == 0);

    const auto& second = decoded.frame(1);
    CHECK(second.id() == 2);

    REQUIRE(second.bounding_box_size() == 1);

    const auto& box = second.bounding_box(0);

    CHECK(box.x() == 0.0f);
    CHECK(box.y() == 0.0f);
    CHECK(box.w() == 1.0f);
    CHECK(box.h() == 1.0f);
    CHECK(box.classid() == 0);
    CHECK(box.confidence() == Approx(0.95f));
    CHECK(box.objectid() == 3);

    REQUIRE(box.has_mask());

    const auto& mask = box.mask();

    CHECK(mask.width() == 4);
    CHECK(mask.height() == 4);

    REQUIRE(mask.rle_size() == 3);
    CHECK(mask.rle(0) == 0);
    CHECK(mask.rle(1) == 6);
    CHECK(mask.rle(2) == 10);

    CHECK(decoded.has_last_updated());
}


TEST_CASE("CVObjectMask handles protobuf output edge cases",
          "[effect][object_mask][opencv]") {
    QTemporaryDir directory;
    REQUIRE(directory.isValid());

    ProcessingController controller;
    CVObjectMask processor("{}", controller);

    Json::Value config;

    SECTION("Missing output directory") {
        const QString path =
            directory.filePath("nonexistent/mask.data");

        config["protobuf_data_path"] = path.toStdString();
        processor.SetJsonValue(config);

        CHECK_FALSE(processor.SaveObjMaskData());
        CHECK_FALSE(QFile::exists(path));
    }

    SECTION("Empty frame collection") {
        const QString path =
            directory.filePath("empty_mask.data");

        config["protobuf_data_path"] = path.toStdString();
        processor.SetJsonValue(config);

        REQUIRE(processor.SaveObjMaskData());

        QFile file(path);
        REQUIRE(file.open(QIODevice::ReadOnly));

        const QByteArray bytes = file.readAll();

        pb_objdetect::ObjDetect decoded;
        REQUIRE(decoded.ParseFromArray(
            bytes.constData(), static_cast<int>(bytes.size())));

        CHECK(decoded.frame_size() == 0);
        REQUIRE(decoded.classnames_size() == 1);
        CHECK(decoded.classnames(0) == "object mask");
        CHECK(decoded.has_last_updated());
    }

    SECTION("Repeated save replaces previous frame data") {
        const QString path =
            directory.filePath("repeated_mask.data");

        config["protobuf_data_path"] = path.toStdString();
        processor.SetJsonValue(config);

        CVObjectMaskFrameData frame;
        frame.frameId = 10;
        processor.masksData[10] = frame;

        REQUIRE(processor.SaveObjMaskData());

        processor.masksData.clear();

        REQUIRE(processor.SaveObjMaskData());

        QFile file(path);
        REQUIRE(file.open(QIODevice::ReadOnly));

        const QByteArray bytes = file.readAll();

        pb_objdetect::ObjDetect decoded;
        REQUIRE(decoded.ParseFromArray(
            bytes.constData(), static_cast<int>(bytes.size())));

        CHECK(decoded.frame_size() == 0);
    }
}



TEST_CASE("CVObjectMask rejects invalid JSON prompts",
          "[effect][object_mask][opencv]") {
    ProcessingController controller;
    Clip clip(std::string(TEST_MEDIA_PATH) + "front3.png");
    CVObjectMask processor("{}", controller);

    Json::Value config;
    config["efficient_sam_model_path"] = "/nonexistent/test.onnx";
    config["protobuf_data_path"] = "unused-mask.data";

    SECTION("Invalid frame identifier") {
        config["object_mask_selection"]["frames"]["invalid"]
              ["positive_points"][0]["x"] = 10.0;
        config["object_mask_selection"]["frames"]["invalid"]
              ["positive_points"][0]["y"] = 20.0;
    }

    SECTION("Negative point coordinates") {
        config["object_mask_selection"]["frames"]["1"]
              ["positive_points"][0]["x"] = -10.0;
        config["object_mask_selection"]["frames"]["1"]
              ["positive_points"][0]["y"] = 20.0;
    }

    SECTION("Incomplete rectangle") {
        config["object_mask_selection"]["frames"]["1"]
              ["positive_rects"][0]["x1"] = 10.0;
        config["object_mask_selection"]["frames"]["1"]
              ["positive_rects"][0]["y1"] = 20.0;
    }

    processor.SetJsonValue(config);
    processor.maskClip(clip);

    CHECK(controller.GetError());
    CHECK(controller.GetErrorMessage() ==
          "Missing positive prompt point for Object Mask preprocessing");
}


TEST_CASE("CVObjectMask accepts reversed rectangle prompt",
          "[effect][object_mask][opencv]") {
    ProcessingController controller;
    Clip clip(std::string(TEST_MEDIA_PATH) + "front3.png");
    CVObjectMask processor("{}", controller);

    Json::Value config;
    config["efficient_sam_model_path"] = "/nonexistent/test.onnx";
    config["protobuf_data_path"] = "unused-mask.data";

    Json::Value& rect =
        config["object_mask_selection"]["frames"]["1"]
              ["positive_rects"][0];

    rect["x1"] = 100.0;
    rect["y1"] = 120.0;
    rect["x2"] = 20.0;
    rect["y2"] = 30.0;

    processor.SetJsonValue(config);
    processor.maskClip(clip);

    CHECK(controller.GetError());
    CHECK(controller.GetErrorMessage().find(
        "Failed to load ONNX model:") != std::string::npos);
}


TEST_CASE("CVObjectMask accepts mixed JSON prompts",
          "[effect][object_mask][opencv]") {
    ProcessingController controller;
    Clip clip(std::string(TEST_MEDIA_PATH) + "front3.png");
    CVObjectMask processor("{}", controller);

    Json::Value config;
    config["efficient_sam_model_path"] = "/nonexistent/test.onnx";
    config["protobuf_data_path"] = "unused-mask.data";

    Json::Value& frame =
        config["object_mask_selection"]["frames"]["1"];

    frame["positive_points"][0] = "invalid";
    frame["positive_points"][1]["x"] = 15.0;
    frame["positive_points"][1]["y"] = 25.0;
    frame["positive_rects"][0]["x1"] = 10.0;
    frame["positive_rects"][0]["y1"] = 20.0;

    processor.SetJsonValue(config);
    processor.maskClip(clip);

    CHECK(controller.GetError());
    CHECK(controller.GetErrorMessage().find(
        "Failed to load ONNX model:") != std::string::npos);
}

TEST_CASE("CVObjectMask validates positive prompt",
          "[effect][object_mask][opencv]") {
    ProcessingController controller;
    Clip clip(std::string(TEST_MEDIA_PATH) + "front3.png");
    CVObjectMask processor("{}", controller);

    Json::Value config;
    config["efficient_sam_model_path"] =
        "/nonexistent/openshot-test-efficient-sam.onnx";
    config["protobuf_data_path"] = "unused-mask.data";
    config["positive_x"] = 12.0;
    config["positive_y"] = 24.0;

    processor.SetJsonValue(config);
    processor.maskClip(clip);

    CHECK(controller.GetError());
    CHECK(controller.GetErrorMessage().find(
        "Failed to load ONNX model:") != std::string::npos);
}

TEST_CASE("CVObjectMask accepts legacy JSON prompts",
          "[effect][object_mask][opencv]") {
    ProcessingController controller;
    CVObjectMask processor("{}", controller);

    Json::Value config;
    config["positive_x"] = 1.0;
    config["positive_y"] = 2.0;
    config["negative_x"] = 3.0;
    config["negative_y"] = 4.0;
    config["prompt_slots"] = 99;
    config["mask_threshold"] = 0.5;
    config["model_size"] = 512;

    processor.SetJsonValue(config);

    auto frame = make_object_mask_frame(1, 4, 4);
    CHECK(processor.PreviewSeedMask(frame) == nullptr);

    processor.SetJson("{invalid json");
    CHECK(processor.PreviewSeedMask(nullptr) == nullptr);
}
#endif

#ifdef USE_OPENCV
TEST_CASE("CVObjectMask validates a single EfficientSAM ONNX model path", "[effect][object_mask][opencv]") {
	const std::string error = CVObjectMask::ValidateONNXModel("/tmp/libopenshot_missing_efficientsam.onnx");
	CHECK(error.find("Failed to load ONNX model") != std::string::npos);
}
#endif
