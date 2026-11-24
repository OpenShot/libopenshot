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
#include "Clip.h"
#include "effects/Mask.h"
#include "effects/Negate.h"
#ifdef OPENSHOT_ENABLE_GPU_FRAME
#include "VulkanCompositor.h"
#include "VulkanFrameImage.h"
#include "GpuDevices.h"
extern "C" {
#include <libavutil/buffer.h>
}
#endif
#ifdef USE_OPENCV
#include "CVObjectMask.h"
#endif

#include <QColor>
#include <QFile>
#include <QImage>
#include <QPainter>
#include <QTemporaryDir>

#include <algorithm>
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
TEST_CASE("CVObjectMask validates a single EfficientSAM ONNX model path", "[effect][object_mask][opencv]") {
	const std::string error = CVObjectMask::ValidateONNXModel("/tmp/libopenshot_missing_efficientsam.onnx");
	CHECK(error.find("Failed to load ONNX model") != std::string::npos);
}
#endif

// Compare generated masks with the same artwork rendered as an ordinary clip.
// This catches stretching onto the canvas without the clip's letterbox/transform.
namespace {
class ObjectMaskConsumer : public Mask {
public:
	using EffectBase::ResolveMaskImage;
	using EffectBase::ResolveGeneratedMaskSurface;
};
}

TEST_CASE("Object Mask sources follow clip geometry after transforms", "[effect][object_mask][alignment]") {
	QTemporaryDir directory;
	REQUIRE(directory.isValid());
	const auto data_path = directory.filePath("mask.pb");
	create_object_mask_data(data_path);
	ObjectMask provider;
	provider.Id("object-mask-source");
	provider.draw_mask = Keyframe(0);
	REQUIRE(provider.LoadObjMaskData(data_path.toStdString()));

	const auto native_canvas = std::make_shared<QImage>(40, 20, QImage::Format_RGBA8888_Premultiplied);
	const auto artwork = provider.TrackedObjectMask(native_canvas, 1);
	const auto artwork_path = directory.filePath("mask.png");
	REQUIRE(artwork->save(artwork_path));
	Clip clip(artwork_path.toStdString());
	clip.Open();
	clip.AddEffect(&provider);
	ObjectMaskConsumer consumer;
	consumer.ParentClip(&clip);
	consumer.MaskSourceId(provider.Id());

	SECTION("letterbox") {}
	SECTION("position") { clip.location_x = Keyframe(.2); clip.location_y = Keyframe(-.1); }
	SECTION("rotation") { clip.rotation = Keyframe(90); }
	SECTION("scale") { clip.scale_x = Keyframe(.5); clip.scale_y = Keyframe(.5); }
	SECTION("gravity") { clip.gravity = GRAVITY_BOTTOM; }
	SECTION("crop to fill") { clip.scale = SCALE_CROP; }
	SECTION("Scale None") { clip.scale = SCALE_NONE; }
	SECTION("legacy Scale None") { clip.scale = SCALE_NONE; clip.scale_none_legacy = true; }

	const auto target = std::make_shared<QImage>(80, 80, QImage::Format_RGBA8888_Premultiplied);
	consumer.info.apply_before_clip = true;
	// The default path retains the existing source-space mapping.
	CHECK(*consumer.ResolveMaskImage(native_canvas, 1) == *artwork);
	consumer.info.apply_before_clip = false;

	auto background = std::make_shared<Frame>(1, 80, 80, "#000000");
	const auto backend = qgetenv("OPENSHOT_RENDER_BACKEND");
	qputenv("OPENSHOT_RENDER_BACKEND", "cpu");
	auto reference_frame = clip.GetFrame(background, 1);
	if (backend.isNull()) qunsetenv("OPENSHOT_RENDER_BACKEND");
	else qputenv("OPENSHOT_RENDER_BACKEND", backend);
	QImage expected(80, 80, QImage::Format_RGBA8888_Premultiplied);
	expected.fill(Qt::black);
	QPainter painter(&expected);
	painter.drawImage(0, 0, *reference_frame->GetImage());
	painter.end();
	const auto actual = consumer.ResolveMaskImage(target, 1);
	REQUIRE(actual);
	CHECK(*actual == expected);

	Negate linked_effect;
	linked_effect.ParentClip(&clip);
	linked_effect.MaskSourceId(provider.Id());
	linked_effect.info.apply_before_clip = false;
	qputenv("OPENSHOT_RENDER_BACKEND", "cpu");
	auto cpu_frame = std::make_shared<Frame>(1, 80, 80, "#000000");
	linked_effect.ProcessFrame(cpu_frame, 1);
	if (backend.isNull()) qunsetenv("OPENSHOT_RENDER_BACKEND");
	else qputenv("OPENSHOT_RENDER_BACKEND", backend);
	const auto linked_cpu = cpu_frame->GetImage();
	int cpu_error = 0;
	for (int y = 0; y < 80; ++y)
		for (int x = 0; x < 80; ++x)
			cpu_error = std::max(cpu_error,
				std::abs(linked_cpu->pixelColor(x, y).red() - expected.pixelColor(x, y).red()));
	CHECK(cpu_error <= 1); // Common mask blending truncates floating-point values.

#ifdef OPENSHOT_ENABLE_GPU_FRAME
	// Loading Vulkan does not guarantee a usable GPU (for example on CI).
	// Always run the CPU assertions above; only run GPU assertions with a device.
	AVBufferRef* device = nullptr;
	const bool gpu_available = VulkanRenderingEnabled() && CreateSharedVulkanDevice(&device) >= 0;
	av_buffer_unref(&device);
	if (gpu_available) {
		auto surface = consumer.ResolveGeneratedMaskSurface(SolidVulkanImage(80, 80, Qt::black), 1);
		REQUIRE(surface);
		const auto gpu = surface->Materialize();
		auto gpu_frame = std::make_shared<Frame>(1, 80, 80, "#000000");
		gpu_frame->AddDeferredImage(SolidVulkanImage(80, 80, Qt::black));
		const auto readbacks = VulkanFrameReadbackCount();
		linked_effect.ProcessFrame(gpu_frame, 1);
		REQUIRE(gpu_frame->GetDeferredImage());
		CHECK(VulkanFrameReadbackCount() == readbacks);
		const auto linked_pixels = gpu_frame->GetDeferredImage()->Materialize();
		int maximum_error = 0;
		for (int y = 0; y < 80; ++y)
			for (int x = 0; x < 80; ++x)
				for (const auto* pixels : {&gpu, &linked_pixels})
					maximum_error = std::max(maximum_error,
						std::abs(pixels->pixelColor(x, y).red() - expected.pixelColor(x, y).red()));
		CHECK(maximum_error <= 2);
	}
#endif
	clip.Close();
}
