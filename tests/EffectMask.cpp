/**
 * @file
 * @brief Unit tests for common EffectBase mask dispatch and blur mask modes
 */

// Copyright (c) 2008-2026 OpenShot Studios, LLC
//
// SPDX-License-Identifier: LGPL-3.0-or-later

#include <cstdlib>
#include <cstdio>
#include <cstring>
#include <memory>
#include <fstream>
#include <sstream>
#include <string>
#include <vector>
#include <unistd.h>

#include <QColor>
#include <QDir>
#include <QImage>

#include "CacheMemory.h"
#include "Clip.h"
#include "CVObjectDetection.h"
#include "DummyReader.h"
#include "effects/Blur.h"
#include "effects/Brightness.h"
#include "effects/Hue.h"
#include "effects/ObjectDetection.h"
#include "effects/Pixelate.h"
#include "effects/Saturation.h"
#include "effects/Sharpen.h"
#include "effects/Tracker.h"
#include "TrackedObjectBBox.h"
#include "ProcessingController.h"
#include "QtImageReader.h"
#include "openshot_catch.h"

using namespace openshot;

static std::string temp_png_path(const std::string& base) {
	std::stringstream path;
	path << QDir::tempPath().toStdString() << "/libopenshot_" << base << "_"
		 << getpid() << "_" << rand() << ".png";
	return path.str();
}

TEST_CASE("CVObjectDetection normalizes saved track classes", "[libopenshot][opencv][objectdetection]")
{
	const std::string protobuf_path = temp_png_path("objdetector_class_normalize") + ".data";
	const std::string effect_info = "{\"protobuf_data_path\": \"" + protobuf_path + "\"}";
	ProcessingController controller;
	CVObjectDetection writer(effect_info, controller);

	for (size_t frame = 1; frame <= 3; ++frame) {
		writer.detectionsData[frame] = CVDetectionData(
			{5}, {0.90f}, {cv::Rect_<float>(0.1f, 0.1f, 0.2f, 0.2f)}, frame, {1});
	}
	for (size_t frame = 4; frame <= 13; ++frame) {
		writer.detectionsData[frame] = CVDetectionData(
			{2}, {0.40f}, {cv::Rect_<float>(0.1f, 0.1f, 0.2f, 0.2f)}, frame, {1});
	}

	REQUIRE(writer.SaveObjDetectedData());

	CVObjectDetection reader(effect_info, controller);
	REQUIRE(reader._LoadObjDetectdData());

	CHECK(reader.GetDetectionData(1).classIds.at(0) == 2);
	CHECK(reader.GetDetectionData(13).classIds.at(0) == 2);

	std::remove(protobuf_path.c_str());
}

static std::string create_source_png(int w, int h, const QColor& color) {
	const std::string path = temp_png_path("source");
	QImage image(w, h, QImage::Format_RGBA8888_Premultiplied);
	image.fill(color);
	REQUIRE(image.save(QString::fromStdString(path)));
	return path;
}

static std::string create_mask_png(const std::vector<int>& gray_values) {
	const std::string path = temp_png_path("mask");
	QImage mask(static_cast<int>(gray_values.size()), 1, QImage::Format_RGBA8888_Premultiplied);
	for (size_t i = 0; i < gray_values.size(); ++i) {
		const int gray = gray_values[i];
		mask.setPixelColor(static_cast<int>(i), 0, QColor(gray, gray, gray, 255));
	}
	REQUIRE(mask.save(QString::fromStdString(path)));
	return path;
}

static std::string create_uniform_mask_png(int width, int height, int gray_value) {
	const std::string path = temp_png_path("mask_uniform");
	QImage mask(width, height, QImage::Format_RGBA8888_Premultiplied);
	mask.fill(QColor(gray_value, gray_value, gray_value, 255));
	REQUIRE(mask.save(QString::fromStdString(path)));
	return path;
}

class TrackingMaskReader : public ReaderBase {
private:
	bool is_open = false;
	CacheMemory cache;
	int width = 2;
	int height = 1;

public:
	std::vector<int64_t> requests;

	TrackingMaskReader(int fps_num, int fps_den, int64_t length_frames) {
		info.has_video = true;
		info.has_audio = false;
		info.has_single_image = false;
		info.width = width;
		info.height = height;
		info.fps = Fraction(fps_num, fps_den);
		info.video_length = length_frames;
		info.duration = static_cast<float>(length_frames / info.fps.ToDouble());
		info.sample_rate = 48000;
		info.channels = 2;
		info.audio_stream_index = -1;
	}

	openshot::CacheBase* GetCache() override { return &cache; }
	bool IsOpen() override { return is_open; }
	std::string Name() override { return "TrackingMaskReader"; }
	void Open() override { is_open = true; }
	void Close() override { is_open = false; }

	std::shared_ptr<openshot::Frame> GetFrame(int64_t number) override {
		requests.push_back(number);
		auto frame = std::make_shared<Frame>(number, width, height, "#00000000");
		frame->GetImage()->fill(QColor(128, 128, 128, 255));
		return frame;
	}

	std::string Json() const override {
		return JsonValue().toStyledString();
	}

	Json::Value JsonValue() const override {
		Json::Value root = ReaderBase::JsonValue();
		root["type"] = "TrackingMaskReader";
		root["path"] = "";
		return root;
	}

	void SetJson(const std::string value) override {
		(void) value;
	}

	void SetJsonValue(const Json::Value root) override {
		ReaderBase::SetJsonValue(root);
	}
};

static std::shared_ptr<Frame> make_input_frame(int64_t number, int width = 2, int height = 1) {
	auto frame = std::make_shared<Frame>(number, width, height, "#00000000");
	frame->GetImage()->fill(QColor(64, 64, 64, 255));
	return frame;
}

static void append_varint(std::string& output, uint32_t value) {
	while (value > 0x7f) {
		output.push_back(static_cast<char>((value & 0x7f) | 0x80));
		value >>= 7;
	}
	output.push_back(static_cast<char>(value));
}

static void append_fixed32_float(std::string& output, float value) {
	uint32_t bits = 0;
	static_assert(sizeof(bits) == sizeof(value), "Unexpected float size");
	std::memcpy(&bits, &value, sizeof(bits));
	for (int i = 0; i < 4; ++i)
		output.push_back(static_cast<char>((bits >> (8 * i)) & 0xff));
}

static void append_length_delimited(std::string& output, uint32_t field_number, const std::string& value) {
	append_varint(output, (field_number << 3) | 2);
	append_varint(output, static_cast<uint32_t>(value.size()));
	output.append(value);
}

static std::string create_object_detection_data_with_mask() {
	const std::string path = temp_png_path("object_detection_mask") + ".data";
	std::string mask;
	append_varint(mask, 8);
	append_varint(mask, 2);
	append_varint(mask, 16);
	append_varint(mask, 2);
	for (uint32_t count : {0u, 1u, 3u}) {
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
	append_length_delimited(data, 3, "person");

	std::ofstream output(path, std::ios::out | std::ios::binary);
	output.write(data.data(), static_cast<std::streamsize>(data.size()));
	REQUIRE(output.good());
	return path;
}

TEST_CASE("EffectBase common mask blend applies to ProcessFrame", "[effect][mask][base]") {
	auto frame = std::make_shared<Frame>(1, 4, 1, "#000000");
	auto image = frame->GetImage();
	image->fill(QColor(80, 80, 80, 255));

	const std::string mask_path = create_mask_png({255, 255, 0, 0});

	Brightness effect(Keyframe(0.5), Keyframe(0.0));
	effect.MaskReader(new QtImageReader(mask_path));

	auto out = effect.ProcessFrame(frame, 1);
	auto out_image = out->GetImage();

	CHECK(out_image->pixelColor(0, 0).red() > 80);
	CHECK(out_image->pixelColor(1, 0).red() > 80);
	CHECK(out_image->pixelColor(2, 0).red() == 80);
	CHECK(out_image->pixelColor(3, 0).red() == 80);
}

TEST_CASE("ObjectDetection all object update applies sparse style keys", "[effect][object_detection]") {
	ObjectDetection effect;
	auto first = std::make_shared<TrackedObjectBBox>(10, 20, 30, 255);
	auto second = std::make_shared<TrackedObjectBBox>(200, 210, 220, 255);
	first->AddBox(1, 0.25f, 0.25f, 0.2f, 0.2f, 0.0f);
	second->AddBox(1, 0.75f, 0.75f, 0.2f, 0.2f, 0.0f);
	effect.trackedObjects[1] = first;
	effect.trackedObjects[2] = second;

	Json::Value update;
	update["objects"]["all"]["background_alpha"] = Keyframe(0.0).JsonValue();
	update["objects"]["all"]["stroke_width"] = Keyframe(5.0).JsonValue();
	effect.SetJsonValue(update);

	CHECK(first->background_alpha.GetValue(1) == Approx(0.0));
	CHECK(second->background_alpha.GetValue(1) == Approx(0.0));
	CHECK(first->stroke_width.GetValue(1) == Approx(5.0));
	CHECK(second->stroke_width.GetValue(1) == Approx(5.0));

	CHECK(first->stroke.red.GetValue(1) == Approx(10.0));
	CHECK(second->stroke.red.GetValue(1) == Approx(200.0));
	CHECK(first->background.red.GetValue(1) == Approx(10.0));
	CHECK(second->background.red.GetValue(1) == Approx(200.0));
}

TEST_CASE("ObjectDetection object mask overlay does not fill the whole frame", "[effect][object_detection][mask]") {
	ObjectDetection effect;
	Clip parent_clip;
	effect.ParentClip(&parent_clip);
	const std::string protobuf_path = create_object_detection_data_with_mask();

	Json::Value config;
	config["protobuf_data_path"] = protobuf_path;
	config["display_boxes"] = Keyframe(0.0).JsonValue();
	config["display_box_text"] = Keyframe(0.0).JsonValue();
	effect.SetJsonValue(config);

	const Json::Value properties = stringToJson(effect.PropertiesJSON(1));
	REQUIRE(properties["objects"].isMember("all"));
	REQUIRE(properties["objects"]["all"].isMember("draw_mask"));
	CHECK(properties["objects"]["all"]["draw_mask"]["name"].asString() == "Draw Mask");
	CHECK(properties["objects"]["all"]["draw_mask"]["value"].asBool());
	REQUIRE(properties["objects"]["all"].isMember("mask_color"));
	CHECK(properties["objects"]["all"]["mask_color"]["name"].asString() == "Mask Color");
	REQUIRE(properties["objects"]["all"].isMember("mask_alpha"));
	CHECK(properties["objects"]["all"]["mask_alpha"]["name"].asString() == "Mask Alpha");
	CHECK(properties["objects"]["all"]["mask_alpha"]["value"].asDouble() == Approx(120.0 / 255.0));

	auto frame = make_input_frame(1, 4, 4);
	auto output = effect.GetFrame(frame, 1)->GetImage();

	CHECK(output->pixelColor(0, 0) != QColor(64, 64, 64, 255));
	CHECK(output->pixelColor(3, 3) == QColor(64, 64, 64, 255));

	std::remove(protobuf_path.c_str());
}

TEST_CASE("ObjectDetection all object mask controls survive style updates", "[effect][object_detection][mask]") {
	ObjectDetection effect;
	const std::string protobuf_path = create_object_detection_data_with_mask();

	Json::Value config;
	config["protobuf_data_path"] = protobuf_path;
	effect.SetJsonValue(config);

	Json::Value update;
	update["objects"]["all"]["draw_mask"] = Keyframe(0.0).JsonValue();
	update["objects"]["all"]["mask_alpha"] = Keyframe(0.33).JsonValue();
	effect.SetJsonValue(update);

	Json::Value properties = stringToJson(effect.PropertiesJSON(1));
	REQUIRE(properties["objects"].isMember("all"));
	CHECK(properties["objects"]["all"].isMember("draw_mask"));
	CHECK(properties["objects"]["all"].isMember("mask_color"));
	CHECK(properties["objects"]["all"].isMember("mask_alpha"));
	CHECK(properties["objects"]["all"]["draw_mask"]["value"].asBool() == false);
	CHECK(properties["objects"]["all"]["mask_alpha"]["value"].asDouble() == Approx(0.33));

	std::remove(protobuf_path.c_str());
}

TEST_CASE("ObjectDetection all object mask controls use any masked object", "[effect][object_detection][mask]") {
	ObjectDetection effect;
	auto first = std::make_shared<TrackedObjectBBox>(10, 20, 30, 255);
	auto second = std::make_shared<TrackedObjectBBox>(200, 210, 220, 255);
	first->AddBox(1, 0.25f, 0.25f, 0.2f, 0.2f, 0.0f);
	second->AddBox(1, 0.75f, 0.75f, 0.2f, 0.2f, 0.0f);

	ObjectMaskData mask;
	mask.width = 2;
	mask.height = 2;
	mask.rle = {0, 1, 3};
	second->AddMask(1, mask);

	effect.trackedObjects[1] = first;
	effect.trackedObjects[2] = second;
	effect.selectedObjectIndex = -1;

	Json::Value properties = stringToJson(effect.PropertiesJSON(1));
	REQUIRE(properties["objects"].isMember("all"));
	CHECK(properties["objects"]["all"].isMember("draw_mask"));
	CHECK(properties["objects"]["all"].isMember("mask_color"));
	CHECK(properties["objects"]["all"].isMember("mask_alpha"));
}

TEST_CASE("TrackedObjectBBox reuses nearby masks for short detection gaps", "[effect][object_detection][mask]") {
	TrackedObjectBBox tracked;
	tracked.SetBaseFPS(Fraction(30, 1));
	tracked.AddBox(10, 0.25f, 0.25f, 0.2f, 0.2f, 0.0f);
	tracked.AddBox(11, 0.29f, 0.25f, 0.2f, 0.2f, 0.0f);
	tracked.AddBox(15, 0.45f, 0.25f, 0.2f, 0.2f, 0.0f);
	tracked.AddBox(16, 0.49f, 0.25f, 0.2f, 0.2f, 0.0f);

	ObjectMaskData mask;
	mask.width = 10;
	mask.height = 10;
	mask.rle = {22, 1, 77};
	tracked.AddMask(10, mask);

	CHECK(tracked.HasMask(10));
	CHECK_FALSE(tracked.HasMask(11));
	CHECK(tracked.GetMask(11, 5).HasData());
	CHECK(tracked.GetMask(15, 5).HasData());
	CHECK(tracked.GetMask(15, 5).rle != mask.rle);
	CHECK_FALSE(tracked.GetMask(16, 5).HasData());
	CHECK(tracked.HasMaskData());
}

TEST_CASE("ObjectDetection individual style overrides all object style", "[effect][object_detection]") {
	ObjectDetection effect;
	auto first = std::make_shared<TrackedObjectBBox>(10, 20, 30, 255);
	auto second = std::make_shared<TrackedObjectBBox>(200, 210, 220, 255);
	first->Id("effect-1");
	second->Id("effect-2");
	first->AddBox(1, 0.25f, 0.25f, 0.2f, 0.2f, 0.0f);
	second->AddBox(1, 0.75f, 0.75f, 0.2f, 0.2f, 0.0f);
	effect.trackedObjects[1] = first;
	effect.trackedObjects[2] = second;
	effect.selectedObjectIndex = 1;

	Json::Value update;
	update["objects"]["effect-1"]["stroke"]["red"] = Keyframe(255.0).JsonValue();
	update["objects"]["effect-1"]["stroke"]["green"] = Keyframe(0.0).JsonValue();
	update["objects"]["effect-1"]["stroke"]["blue"] = Keyframe(0.0).JsonValue();
	update["objects"]["all"]["stroke"]["red"] = Keyframe(0.0).JsonValue();
	update["objects"]["all"]["stroke"]["green"] = Keyframe(255.0).JsonValue();
	update["objects"]["all"]["stroke"]["blue"] = Keyframe(0.0).JsonValue();
	effect.SetJsonValue(update);

	CHECK(first->stroke.red.GetValue(1) == Approx(255.0));
	CHECK(first->stroke.green.GetValue(1) == Approx(0.0));
	CHECK(first->stroke.blue.GetValue(1) == Approx(0.0));
	CHECK(second->stroke.red.GetValue(1) == Approx(0.0));
	CHECK(second->stroke.green.GetValue(1) == Approx(255.0));
	CHECK(second->stroke.blue.GetValue(1) == Approx(0.0));

	Json::Value props = stringToJson(effect.PropertiesJSON(1));
	REQUIRE(props["objects"].isMember("effect-1"));
	CHECK(props["objects"]["effect-1"]["stroke"]["red"]["value"].asDouble() == Approx(255.0));
	CHECK(props["objects"]["effect-1"]["stroke"]["green"]["value"].asDouble() == Approx(0.0));
	CHECK(props["objects"]["effect-1"]["stroke"]["blue"]["value"].asDouble() == Approx(0.0));
}

TEST_CASE("ObjectDetection all object selection exposes tracked object properties", "[effect][object_detection]") {
	ObjectDetection effect;
	auto tracked = std::make_shared<TrackedObjectBBox>(10, 20, 30, 255);
	tracked->AddBox(1, 0.25f, 0.25f, 0.2f, 0.2f, 0.0f);
	effect.trackedObjects[1] = tracked;
	effect.selectedObjectIndex = -1;

	Json::Value props = stringToJson(effect.PropertiesJSON(1));
	REQUIRE(props["objects"].isMember("all"));
	CHECK(props["selected_object_index"]["min"].asInt() == -1);
	CHECK(props["objects"]["all"].isMember("background_alpha"));
	CHECK(props["objects"]["all"].isMember("stroke"));
	CHECK(props["objects"]["all"].isMember("delta_x"));
	CHECK(props["objects"]["all"].isMember("scale_x"));
	CHECK_FALSE(props["objects"]["all"].isMember("x1"));
	CHECK_FALSE(props.isMember("display_box_text"));
	CHECK_FALSE(props.isMember("display_boxes"));
}

TEST_CASE("ObjectDetection all object properties keep explicit all values", "[effect][object_detection]") {
	ObjectDetection effect;
	auto first = std::make_shared<TrackedObjectBBox>(10, 20, 30, 255);
	auto second = std::make_shared<TrackedObjectBBox>(200, 210, 220, 255);
	first->Id("effect-1");
	second->Id("effect-2");
	first->AddBox(1, 0.25f, 0.25f, 0.2f, 0.2f, 0.0f);
	second->AddBox(1, 0.75f, 0.75f, 0.2f, 0.2f, 0.0f);
	effect.trackedObjects[1] = first;
	effect.trackedObjects[2] = second;

	Json::Value update;
	update["objects"]["all"]["stroke_width"] = Keyframe(6.0).JsonValue();
	update["objects"]["all"]["stroke_alpha"] = Keyframe(0.25).JsonValue();
	update["objects"]["effect-1"]["stroke_width"] = Keyframe(3.0).JsonValue();
	update["objects"]["effect-1"]["stroke_alpha"] = Keyframe(0.75).JsonValue();
	effect.SetJsonValue(update);

	effect.selectedObjectIndex = -1;
	Json::Value all_props = stringToJson(effect.PropertiesJSON(1));
	REQUIRE(all_props["objects"].isMember("all"));
	CHECK(all_props["objects"]["all"]["stroke_width"]["value"].asDouble() == Approx(6.0));
	CHECK(all_props["objects"]["all"]["stroke_alpha"]["value"].asDouble() == Approx(0.25));

	effect.selectedObjectIndex = 1;
	Json::Value first_props = stringToJson(effect.PropertiesJSON(1));
	REQUIRE(first_props["objects"].isMember("effect-1"));
	CHECK(first_props["objects"]["effect-1"]["stroke_width"]["value"].asDouble() == Approx(3.0));
	CHECK(first_props["objects"]["effect-1"]["stroke_alpha"]["value"].asDouble() == Approx(0.75));
}

TEST_CASE("EffectBase mask fields serialize and deserialize", "[effect][mask][json]") {
	const std::string mask_path = create_mask_png({255, 0});

	Brightness effect(Keyframe(0.0), Keyframe(0.0));
	effect.mask_invert = true;
	effect.MaskSourceId("tracker-effect-id");
	effect.MaskReader(new QtImageReader(mask_path));

	const Json::Value json = effect.JsonValue();
	CHECK(json["mask_invert"].asBool());
	CHECK(json["mask_source_id"].asString() == "tracker-effect-id");
	REQUIRE(json["mask_reader"].isObject());
	CHECK(json["mask_reader"]["type"].asString() == "QtImageReader");

	Brightness copy(Keyframe(0.0), Keyframe(0.0));
	copy.SetJsonValue(json);
	CHECK(copy.mask_invert);
	CHECK(copy.MaskSourceId() == "tracker-effect-id");
	CHECK(copy.MaskReader() != nullptr);
}

TEST_CASE("EffectBase can use tracker effect bbox as generated mask source", "[effect][mask][tracker]") {
	Clip parent_clip;
	Tracker tracker;
	tracker.Id("tracker-source");
	tracker.trackedData->SetBaseFPS(Fraction(30, 1));
	tracker.trackedData->AddBox(1, 0.25f, 0.5f, 0.5f, 1.0f, 0.0f);
	tracker.trackedData->draw_box = Keyframe(0.0);

	Brightness brightness(Keyframe(0.8), Keyframe(0.0));
	brightness.MaskSourceId("tracker-source");

	parent_clip.AddEffect(&tracker);
	parent_clip.AddEffect(&brightness);

	auto frame = std::make_shared<Frame>(1, 4, 1, "#000000");
	frame->GetImage()->fill(QColor(80, 80, 80, 255));

	auto out = brightness.ProcessFrame(frame, 1);
	auto out_image = out->GetImage();

	CHECK(out_image->pixelColor(0, 0).red() > 80);
	CHECK(out_image->pixelColor(1, 0).red() > 80);
	CHECK(out_image->pixelColor(2, 0).red() == 80);
	CHECK(out_image->pixelColor(3, 0).red() == 80);
}

TEST_CASE("EffectBase tracker mask source honors corner radius", "[effect][mask][tracker]") {
	Clip parent_clip;
	Tracker tracker;
	tracker.Id("tracker-source");
	tracker.trackedData->SetBaseFPS(Fraction(30, 1));
	tracker.trackedData->AddBox(1, 0.5f, 0.5f, 1.0f, 1.0f, 0.0f);
	tracker.trackedData->background_corner = Keyframe(12.0);

	Brightness brightness(Keyframe(0.8), Keyframe(0.0));
	brightness.MaskSourceId("tracker-source");

	parent_clip.AddEffect(&tracker);
	parent_clip.AddEffect(&brightness);

	auto frame = std::make_shared<Frame>(1, 20, 20, "#000000");
	frame->GetImage()->fill(QColor(80, 80, 80, 255));

	auto out = brightness.ProcessFrame(frame, 1);
	auto out_image = out->GetImage();

	CHECK(out_image->pixelColor(0, 0).red() == 80);
	CHECK(out_image->pixelColor(10, 10).red() > 80);
}

TEST_CASE("EffectBase tracker mask source with no visible bbox preserves original frame", "[effect][mask][tracker]") {
	Clip parent_clip;
	Tracker tracker;
	tracker.Id("tracker-source");
	tracker.trackedData->SetBaseFPS(Fraction(30, 1));
	tracker.trackedData->AddBox(1, 0.25f, 0.5f, 0.5f, 1.0f, 0.0f);
	tracker.trackedData->visible = Keyframe(0.0);

	Brightness brightness(Keyframe(0.8), Keyframe(0.0));
	brightness.MaskSourceId("tracker-source");

	parent_clip.AddEffect(&tracker);
	parent_clip.AddEffect(&brightness);

	auto frame = std::make_shared<Frame>(1, 4, 1, "#000000");
	frame->GetImage()->fill(QColor(80, 80, 80, 255));

	auto out = brightness.ProcessFrame(frame, 1);
	auto out_image = out->GetImage();

	for (int x = 0; x < 4; ++x)
		CHECK(out_image->pixelColor(x, 0).red() == 80);
}

TEST_CASE("Blur mask mode drive amount differs from post blend", "[effect][mask][blur]") {
	const int width = 20;
	const int height = 20;
	auto frame_post = std::make_shared<Frame>(1, width, height, "#000000");
	auto frame_drive = std::make_shared<Frame>(1, width, height, "#000000");

	QImage input(width, height, QImage::Format_RGBA8888_Premultiplied);
	for (int y = 0; y < height; ++y) {
		for (int x = 0; x < width; ++x) {
			const int alpha = std::min(255, x * 12 + y * 3);
			input.setPixelColor(x, y, QColor(255, 180, 40, alpha));
		}
	}
	*frame_post->GetImage() = input;
	*frame_drive->GetImage() = input;

	const std::string mask_path = create_uniform_mask_png(width, height, 128);

	Blur post(Keyframe(3.0), Keyframe(3.0), Keyframe(3.0), Keyframe(1.0));
	post.mask_mode = BLUR_MASK_POST_BLEND;
	post.MaskReader(new QtImageReader(mask_path));

	Blur drive(Keyframe(3.0), Keyframe(3.0), Keyframe(3.0), Keyframe(1.0));
	drive.mask_mode = BLUR_MASK_DRIVE_AMOUNT;
	drive.MaskReader(new QtImageReader(mask_path));

	auto out_post = post.ProcessFrame(frame_post, 1);
	auto out_drive = drive.ProcessFrame(frame_drive, 1);

	bool any_diff = false;
	for (int y = 0; y < height && !any_diff; ++y) {
		for (int x = 0; x < width; ++x) {
			if (out_post->GetImage()->pixelColor(x, y) != out_drive->GetImage()->pixelColor(x, y)) {
				any_diff = true;
				break;
			}
		}
	}
	if (!any_diff) {
		for (int x = 0; x < width; ++x) {
			if (out_post->GetImage()->pixelColor(x, height / 2) != out_drive->GetImage()->pixelColor(x, height / 2)) {
			any_diff = true;
			break;
		}
	}
	}
	CHECK(any_diff);
}

TEST_CASE("Saturation mask mode drive amount differs from post blend", "[effect][mask][saturation]") {
	auto frame_post = std::make_shared<Frame>(1, 1, 1, "#000000");
	auto frame_drive = std::make_shared<Frame>(1, 1, 1, "#000000");

	QImage input(1, 1, QImage::Format_RGBA8888_Premultiplied);
	input.setPixelColor(0, 0, QColor(70, 120, 200, 255));
	*frame_post->GetImage() = input;
	*frame_drive->GetImage() = input;

	const std::string mask_path = create_mask_png({128});

	Saturation post(Keyframe(2.0), Keyframe(1.0), Keyframe(1.0), Keyframe(1.0));
	post.mask_mode = SATURATION_MASK_POST_BLEND;
	post.MaskReader(new QtImageReader(mask_path));

	Saturation drive(Keyframe(2.0), Keyframe(1.0), Keyframe(1.0), Keyframe(1.0));
	drive.mask_mode = SATURATION_MASK_DRIVE_AMOUNT;
	drive.MaskReader(new QtImageReader(mask_path));

	auto out_post = post.ProcessFrame(frame_post, 1);
	auto out_drive = drive.ProcessFrame(frame_drive, 1);

	CHECK(out_post->GetImage()->pixelColor(0, 0) != out_drive->GetImage()->pixelColor(0, 0));
}

TEST_CASE("Brightness mask mode vary strength differs from limit-to-area", "[effect][mask][brightness]") {
	auto frame_post = std::make_shared<Frame>(1, 1, 1, "#000000");
	auto frame_drive = std::make_shared<Frame>(1, 1, 1, "#000000");

	QImage input(1, 1, QImage::Format_RGBA8888_Premultiplied);
	input.setPixelColor(0, 0, QColor(80, 120, 200, 255));
	*frame_post->GetImage() = input;
	*frame_drive->GetImage() = input;

	const std::string mask_path = create_mask_png({128});

	Brightness post(Keyframe(0.6), Keyframe(6.0));
	post.mask_mode = BRIGHTNESS_MASK_LIMIT_TO_AREA;
	post.MaskReader(new QtImageReader(mask_path));

	Brightness drive(Keyframe(0.6), Keyframe(6.0));
	drive.mask_mode = BRIGHTNESS_MASK_VARY_STRENGTH;
	drive.MaskReader(new QtImageReader(mask_path));

	auto out_post = post.ProcessFrame(frame_post, 1);
	auto out_drive = drive.ProcessFrame(frame_drive, 1);

	CHECK(out_post->GetImage()->pixelColor(0, 0) != out_drive->GetImage()->pixelColor(0, 0));
}

TEST_CASE("Hue mask mode vary strength differs from limit-to-area", "[effect][mask][hue]") {
	auto frame_post = std::make_shared<Frame>(1, 1, 1, "#000000");
	auto frame_drive = std::make_shared<Frame>(1, 1, 1, "#000000");

	QImage input(1, 1, QImage::Format_RGBA8888_Premultiplied);
	input.setPixelColor(0, 0, QColor(200, 80, 40, 255));
	*frame_post->GetImage() = input;
	*frame_drive->GetImage() = input;

	const std::string mask_path = create_mask_png({128});

	Hue post(Keyframe(0.8));
	post.mask_mode = HUE_MASK_LIMIT_TO_AREA;
	post.MaskReader(new QtImageReader(mask_path));

	Hue drive(Keyframe(0.8));
	drive.mask_mode = HUE_MASK_VARY_STRENGTH;
	drive.MaskReader(new QtImageReader(mask_path));

	auto out_post = post.ProcessFrame(frame_post, 1);
	auto out_drive = drive.ProcessFrame(frame_drive, 1);

	CHECK(out_post->GetImage()->pixelColor(0, 0) != out_drive->GetImage()->pixelColor(0, 0));
}

TEST_CASE("Pixelate mask mode vary strength differs from limit-to-area", "[effect][mask][pixelate]") {
	const int width = 20;
	const int height = 20;
	auto frame_post = std::make_shared<Frame>(1, width, height, "#000000");
	auto frame_drive = std::make_shared<Frame>(1, width, height, "#000000");

	QImage input(width, height, QImage::Format_RGBA8888_Premultiplied);
	for (int y = 0; y < height; ++y) {
		for (int x = 0; x < width; ++x) {
			input.setPixelColor(x, y, QColor((x * 13) % 256, (y * 11) % 256, ((x + y) * 7) % 256, 255));
		}
	}
	*frame_post->GetImage() = input;
	*frame_drive->GetImage() = input;

	const std::string mask_path = create_uniform_mask_png(width, height, 128);

	Pixelate post(Keyframe(1.0), Keyframe(0.0), Keyframe(0.0), Keyframe(0.0), Keyframe(0.0));
	post.mask_mode = PIXELATE_MASK_LIMIT_TO_AREA;
	post.MaskReader(new QtImageReader(mask_path));

	Pixelate drive(Keyframe(1.0), Keyframe(0.0), Keyframe(0.0), Keyframe(0.0), Keyframe(0.0));
	drive.mask_mode = PIXELATE_MASK_VARY_STRENGTH;
	drive.MaskReader(new QtImageReader(mask_path));

	auto out_post = post.ProcessFrame(frame_post, 1);
	auto out_drive = drive.ProcessFrame(frame_drive, 1);

	bool any_diff = false;
	for (int y = 0; y < height && !any_diff; ++y) {
		for (int x = 0; x < width; ++x) {
			if (out_post->GetImage()->pixelColor(x, y) != out_drive->GetImage()->pixelColor(x, y)) {
				any_diff = true;
				break;
			}
		}
	}
	CHECK(any_diff);
}

TEST_CASE("Sharpen mask mode vary strength differs from limit-to-area", "[effect][mask][sharpen]") {
	const int width = 20;
	const int height = 20;
	auto frame_post = std::make_shared<Frame>(1, width, height, "#000000");
	auto frame_drive = std::make_shared<Frame>(1, width, height, "#000000");

	QImage input(width, height, QImage::Format_RGBA8888_Premultiplied);
	for (int y = 0; y < height; ++y) {
		for (int x = 0; x < width; ++x) {
			const int red = (x < width / 2) ? 30 : 220;
			const int green = (y < height / 2) ? 60 : 200;
			const int blue = ((x + y) % 2 == 0) ? 20 : 240;
			input.setPixelColor(x, y, QColor(red, green, blue, 255));
		}
	}
	*frame_post->GetImage() = input;
	*frame_drive->GetImage() = input;

	const std::string mask_path = create_uniform_mask_png(width, height, 128);

	Sharpen post(Keyframe(1.6), Keyframe(3.0), Keyframe(0.05));
	post.mask_mode = SHARPEN_MASK_LIMIT_TO_AREA;
	post.MaskReader(new QtImageReader(mask_path));

	Sharpen drive(Keyframe(1.6), Keyframe(3.0), Keyframe(0.05));
	drive.mask_mode = SHARPEN_MASK_VARY_STRENGTH;
	drive.MaskReader(new QtImageReader(mask_path));

	auto out_post = post.ProcessFrame(frame_post, 1);
	auto out_drive = drive.ProcessFrame(frame_drive, 1);

	bool any_diff = false;
	for (int y = 0; y < height && !any_diff; ++y) {
		for (int x = 0; x < width; ++x) {
			if (out_post->GetImage()->pixelColor(x, y) != out_drive->GetImage()->pixelColor(x, y)) {
				any_diff = true;
				break;
			}
		}
	}
	CHECK(any_diff);
}

TEST_CASE("Effect mask_mode roundtrip for supported effects", "[effect][mask][mode][json]") {
	Blur blur(Keyframe(1.0), Keyframe(1.0), Keyframe(3.0), Keyframe(1.0));
	blur.mask_mode = BLUR_MASK_DRIVE_AMOUNT;
	Blur blur_copy;
	blur_copy.SetJsonValue(blur.JsonValue());
	CHECK(blur_copy.mask_mode == BLUR_MASK_DRIVE_AMOUNT);

	Saturation saturation(Keyframe(1.0), Keyframe(1.0), Keyframe(1.0), Keyframe(1.0));
	saturation.mask_mode = SATURATION_MASK_DRIVE_AMOUNT;
	Saturation saturation_copy;
	saturation_copy.SetJsonValue(saturation.JsonValue());
	CHECK(saturation_copy.mask_mode == SATURATION_MASK_DRIVE_AMOUNT);

	Brightness brightness(Keyframe(0.4), Keyframe(5.0));
	brightness.mask_mode = BRIGHTNESS_MASK_VARY_STRENGTH;
	Brightness brightness_copy;
	brightness_copy.SetJsonValue(brightness.JsonValue());
	CHECK(brightness_copy.mask_mode == BRIGHTNESS_MASK_VARY_STRENGTH);

	Hue hue(Keyframe(0.4));
	hue.mask_mode = HUE_MASK_VARY_STRENGTH;
	Hue hue_copy;
	hue_copy.SetJsonValue(hue.JsonValue());
	CHECK(hue_copy.mask_mode == HUE_MASK_VARY_STRENGTH);

	Pixelate pixelate(Keyframe(0.8), Keyframe(0.0), Keyframe(0.0), Keyframe(0.0), Keyframe(0.0));
	pixelate.mask_mode = PIXELATE_MASK_VARY_STRENGTH;
	Pixelate pixelate_copy;
	pixelate_copy.SetJsonValue(pixelate.JsonValue());
	CHECK(pixelate_copy.mask_mode == PIXELATE_MASK_VARY_STRENGTH);

	Sharpen sharpen(Keyframe(1.5), Keyframe(2.0), Keyframe(0.1));
	sharpen.mask_mode = SHARPEN_MASK_VARY_STRENGTH;
	Sharpen sharpen_copy;
	sharpen_copy.SetJsonValue(sharpen.JsonValue());
	CHECK(sharpen_copy.mask_mode == SHARPEN_MASK_VARY_STRENGTH);
}

TEST_CASE("EffectBase accepts legacy reader key for mask source", "[effect][mask][json][legacy_reader]") {
	const std::string mask_path = create_mask_png({255, 0});
	QtImageReader reader(mask_path);

	Saturation effect(Keyframe(2.0), Keyframe(1.0), Keyframe(1.0), Keyframe(1.0));
	Json::Value update;
	update["reader"] = reader.JsonValue();
	effect.SetJsonValue(update);

	REQUIRE(effect.MaskReader() != nullptr);
	CHECK(effect.JsonValue()["mask_reader"]["type"].asString() == "QtImageReader");
}

TEST_CASE("EffectBase uses ClipBase start and end for mask source trim", "[effect][mask][timing][json]") {
	Brightness effect(Keyframe(0.0), Keyframe(0.0));

	Json::Value update;
	update["start"] = -10;
	update["end"] = -20;
	update["mask_time_mode"] = 99;
	update["mask_loop_mode"] = 99;
	effect.SetJsonValue(update);

	const Json::Value clamped = effect.JsonValue();
	CHECK(clamped["start"].asDouble() == Approx(-10.0).margin(0.00001));
	CHECK(clamped["end"].asDouble() == Approx(-20.0).margin(0.00001));
	CHECK(clamped["mask_time_mode"].asInt() == 1);
	CHECK(clamped["mask_loop_mode"].asInt() == 0);

	update["start"] = 0.5;
	update["end"] = 1.2;
	update["mask_time_mode"] = 1;
	update["mask_loop_mode"] = 2;
	effect.SetJsonValue(update);
	const Json::Value roundtrip = effect.JsonValue();
	CHECK(roundtrip["start"].asDouble() == Approx(0.5).margin(0.00001));
	CHECK(roundtrip["end"].asDouble() == Approx(1.2).margin(0.00001));
	CHECK(roundtrip["mask_time_mode"].asInt() == 1);
	CHECK(roundtrip["mask_loop_mode"].asInt() == 2);
}

TEST_CASE("EffectBase defaults mask time mode to source FPS", "[effect][mask][timing][default]") {
	Brightness effect(Keyframe(0.0), Keyframe(0.0));
	CHECK(effect.JsonValue()["mask_time_mode"].asInt() == 1);
}

TEST_CASE("EffectBase mask properties use base start and end controls", "[effect][mask][timing][properties]") {
	Brightness effect(Keyframe(0.0), Keyframe(0.0));
	Json::Value update;
	update["start"] = 0.2;
	update["end"] = 0.9;
	update["mask_time_mode"] = 1;
	update["mask_loop_mode"] = 2;
	effect.SetJsonValue(update);

	const Json::Value properties = openshot::stringToJson(effect.PropertiesJSON(1));
	REQUIRE(properties.isObject());
	CHECK(properties["start"]["name"].asString() == "Start");
	CHECK(properties["end"]["name"].asString() == "End");
	CHECK(properties["mask_reader"]["name"].asString() == "Mask: Source");
	CHECK(properties["mask_time_mode"]["name"].asString() == "Mask: Time Mode");
	CHECK(properties["mask_loop_mode"]["name"].asString() == "Mask: Loop");
	CHECK_FALSE(properties.isMember("mask_start"));
	CHECK_FALSE(properties.isMember("mask_end"));
	CHECK(properties["mask_time_mode"]["choices"].size() == 2);
	CHECK(properties["mask_loop_mode"]["choices"].size() == 3);
}

TEST_CASE("EffectBase accepts legacy mask_start and mask_end aliases", "[effect][mask][timing][legacy_aliases]") {
	Brightness effect(Keyframe(0.0), Keyframe(0.0));

	Json::Value update;
	update["mask_start"] = 0.5;
	update["mask_end"] = 1.2;
	effect.SetJsonValue(update);

	const Json::Value roundtrip = effect.JsonValue();
	CHECK(roundtrip["start"].asDouble() == Approx(0.5).margin(0.00001));
	CHECK(roundtrip["end"].asDouble() == Approx(1.2).margin(0.00001));
	CHECK_FALSE(roundtrip.isMember("mask_start"));
	CHECK_FALSE(roundtrip.isMember("mask_end"));
}

TEST_CASE("EffectBase canonical start and end override legacy mask aliases", "[effect][mask][timing][legacy_precedence]") {
	Brightness effect(Keyframe(0.0), Keyframe(0.0));

	Json::Value update;
	update["start"] = 0.25;
	update["end"] = 0.75;
	update["mask_start"] = 1.5;
	update["mask_end"] = 2.5;
	effect.SetJsonValue(update);

	const Json::Value roundtrip = effect.JsonValue();
	CHECK(roundtrip["start"].asDouble() == Approx(0.25).margin(0.00001));
	CHECK(roundtrip["end"].asDouble() == Approx(0.75).margin(0.00001));
}

TEST_CASE("EffectBase timeline mode maps one-to-one with repeat loop", "[effect][mask][timing][timeline][repeat]") {
	auto* tracking = new TrackingMaskReader(24, 1, 100);
	Brightness effect(Keyframe(0.0), Keyframe(0.0));
	effect.MaskReader(tracking);

	Json::Value update;
	update["start"] = 1.0 / 24.0;
	update["end"] = 3.0 / 24.0;
	update["mask_time_mode"] = 0; // Timeline
	update["mask_loop_mode"] = 1; // Repeat
	effect.SetJsonValue(update);

	for (int64_t n = 1; n <= 7; ++n)
		effect.ProcessFrame(make_input_frame(n), n);

	const std::vector<int64_t> expected = {2, 3, 4, 2, 3, 4, 2};
	CHECK(tracking->requests == expected);
}

TEST_CASE("EffectBase timeline mode supports ping-pong loop", "[effect][mask][timing][timeline][pingpong]") {
	auto* tracking = new TrackingMaskReader(24, 1, 100);
	Brightness effect(Keyframe(0.0), Keyframe(0.0));
	effect.MaskReader(tracking);

	Json::Value update;
	update["start"] = 1.0 / 24.0;
	update["end"] = 3.0 / 24.0;
	update["mask_time_mode"] = 0; // Timeline
	update["mask_loop_mode"] = 2; // Ping-Pong
	effect.SetJsonValue(update);

	for (int64_t n = 1; n <= 7; ++n)
		effect.ProcessFrame(make_input_frame(n), n);

	const std::vector<int64_t> expected = {2, 3, 4, 3, 2, 3, 4};
	CHECK(tracking->requests == expected);
}

TEST_CASE("EffectBase timeline mode play-once clamps at end", "[effect][mask][timing][timeline][once]") {
	auto* tracking = new TrackingMaskReader(24, 1, 100);
	Brightness effect(Keyframe(0.0), Keyframe(0.0));
	effect.MaskReader(tracking);

	Json::Value update;
	update["start"] = 1.0 / 24.0;
	update["end"] = 3.0 / 24.0;
	update["mask_time_mode"] = 0; // Timeline
	update["mask_loop_mode"] = 0; // Play Once
	effect.SetJsonValue(update);

	for (int64_t n = 1; n <= 6; ++n)
		effect.ProcessFrame(make_input_frame(n), n);

	const std::vector<int64_t> expected = {2, 3, 4, 4, 4, 4};
	CHECK(tracking->requests == expected);
}

TEST_CASE("EffectBase source FPS mode maps using parent clip FPS", "[effect][mask][timing][source_fps]") {
	DummyReader clip_reader(Fraction(30, 1), 320, 240, 48000, 2, 4.0f);
	Clip parent_clip;
	parent_clip.Reader(&clip_reader);

	auto* tracking = new TrackingMaskReader(15, 1, 100);
	Brightness effect(Keyframe(0.0), Keyframe(0.0));
	effect.ParentClip(&parent_clip);
	effect.MaskReader(tracking);

	Json::Value update;
	update["start"] = 0.0;
	update["end"] = 0;
	update["mask_time_mode"] = 1; // Source FPS
	update["mask_loop_mode"] = 0; // Play Once
	effect.SetJsonValue(update);

	for (int64_t n = 1; n <= 5; ++n)
		effect.ProcessFrame(make_input_frame(n), n);

	const std::vector<int64_t> expected = {1, 2, 2, 3, 3};
	CHECK(tracking->requests == expected);
}

TEST_CASE("Clip-attached effects still process every frame while using trimmed mask source", "[effect][mask][timing][clip_effect]") {
	DummyReader clip_reader(Fraction(24, 1), 320, 240, 48000, 2, 4.0f);
	Clip parent_clip;
	parent_clip.Reader(&clip_reader);

	auto* tracking = new TrackingMaskReader(24, 1, 100);
	Brightness effect(Keyframe(0.0), Keyframe(0.0));
	effect.ParentClip(&parent_clip);
	effect.MaskReader(tracking);
	effect.Start(1.0 / 24.0);
	effect.End(3.0 / 24.0);
	effect.mask_loop_mode = EffectBase::MASK_LOOP_PLAY_ONCE;

	for (int64_t n = 1; n <= 6; ++n)
		effect.ProcessFrame(make_input_frame(n), n);

	const std::vector<int64_t> expected = {2, 3, 4, 4, 4, 4};
	CHECK(tracking->requests == expected);
}
