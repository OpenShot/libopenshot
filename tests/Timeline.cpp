/**
 * @file
 * @brief Unit tests for openshot::Timeline
 * @author Jonathan Thomas <jonathan@openshot.org>
 *
 * @ref License
 */

// Copyright (c) 2008-2019 OpenShot Studios, LLC
//
// SPDX-License-Identifier: LGPL-3.0-or-later

#include <string>
#include <sstream>
#include <memory>
#include <list>
#include <vector>
#include <cstdint>
#include <omp.h>

#include "openshot_catch.h"

#include "FrameMapper.h"
#include "Timeline.h"
#include "Clip.h"
#include "CacheMemory.h"
#include "DummyReader.h"
#include "Frame.h"
#include "Fraction.h"
#include "effects/Brightness.h"
#include "Exceptions.h"
#include "effects/Blur.h"
#include "effects/Bars.h"
#include "effects/Mask.h"
#include "effects/Negate.h"
#include "effects/Saturation.h"

using namespace openshot;

TEST_CASE("Deleting a JSON-owned clip invalidates its former range", "[libopenshot][timeline][sentry-delete]")
{
	Timeline timeline(2, 2, Fraction(30, 1), 44100, 2, LAYOUT_STEREO);
	DummyReader reader;
	Clip source(&reader);
	source.Id("owned");
	source.Position(1.0);
	source.End(1.0);
	Json::Value changes(Json::arrayValue);
	Json::Value change;
	change["type"] = "insert";
	change["key"].append("clips");
	change["value"] = source.JsonValue();
	changes.append(change);
	timeline.ApplyJsonDiff(changes.toStyledString());
	REQUIRE(timeline.GetClip("owned") != nullptr);

	for (int number : {5, 45, 90})
		timeline.GetCache()->Add(std::make_shared<Frame>(number, 2, 2, "black"));
	REQUIRE(timeline.GetCache()->Contains(45));

	changes[0]["type"] = "delete";
	Json::Value id;
	id["id"] = "owned";
	changes[0]["key"].append(id);
	changes[0]["value"] = Json::nullValue;
	timeline.ApplyJsonDiff(changes.toStyledString());
	CHECK(timeline.GetClip("owned") == nullptr);
	CHECK_FALSE(timeline.GetCache()->Contains(45));
	CHECK(timeline.GetCache()->Contains(5));
	CHECK(timeline.GetCache()->Contains(90));
	// Duplicate delayed deletes are harmless.
	CHECK_NOTHROW(timeline.ApplyJsonDiff(changes.toStyledString()));
}

static uint64_t image_fingerprint(const std::shared_ptr<QImage>& image) {
	const uint64_t kFnvOffset = 1469598103934665603ULL;
	const uint64_t kFnvPrime = 1099511628211ULL;
	uint64_t hash = kFnvOffset;

	if (!image) {
		return hash;
	}

	const unsigned char* bytes = image->constBits();
	const size_t count = static_cast<size_t>(image->sizeInBytes());
	for (size_t i = 0; i < count; ++i) {
		hash ^= static_cast<uint64_t>(bytes[i]);
		hash *= kFnvPrime;
	}

	return hash;
}

class TimelineTrackingMaskReader : public ReaderBase {
private:
	bool is_open = false;
	CacheMemory cache;
	int width = 2;
	int height = 1;

public:
	std::vector<int64_t> requests;

	TimelineTrackingMaskReader(int fps_num, int fps_den, int64_t length_frames) {
		info.has_video = true;
		info.has_audio = false;
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
	std::string Name() override { return "TimelineTrackingMaskReader"; }
	void Open() override { is_open = true; }
	void Close() override { is_open = false; }

	std::shared_ptr<openshot::Frame> GetFrame(int64_t number) override {
		requests.push_back(number);
		auto frame = std::make_shared<Frame>(number, width, height, "#00000000");
		frame->GetImage()->fill(QColor(128, 128, 128, 255));
		return frame;
	}

	std::string Json() const override { return JsonValue().toStyledString(); }
	Json::Value JsonValue() const override {
		Json::Value root = ReaderBase::JsonValue();
		root["type"] = "TimelineTrackingMaskReader";
		root["path"] = "";
		return root;
	}
	void SetJson(const std::string value) override { (void) value; }
	void SetJsonValue(const Json::Value root) override { ReaderBase::SetJsonValue(root); }
};

class TimelineSolidColorReader : public ReaderBase {
private:
	bool is_open = false;
	CacheMemory cache;
	QColor color;

public:
	TimelineSolidColorReader(int width,
	                         int height,
	                         int fps_num,
	                         int fps_den,
	                         int64_t length_frames,
	                         const QColor& fill_color)
		: color(fill_color) {
		info.has_video = true;
		info.has_audio = false;
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
	std::string Name() override { return "TimelineSolidColorReader"; }
	void Open() override { is_open = true; }
	void Close() override { is_open = false; }

	std::shared_ptr<openshot::Frame> GetFrame(int64_t number) override {
		auto frame = std::make_shared<Frame>(number, info.width, info.height, "#00000000");
		frame->GetImage()->fill(color);
		return frame;
	}

	std::string Json() const override { return JsonValue().toStyledString(); }
	Json::Value JsonValue() const override {
		Json::Value root = ReaderBase::JsonValue();
		root["type"] = "TimelineSolidColorReader";
		root["path"] = "";
		return root;
	}
	void SetJson(const std::string value) override { (void) value; }
	void SetJsonValue(const Json::Value root) override { ReaderBase::SetJsonValue(root); }
};

class TimelineConstantAudioReader : public ReaderBase {
private:
	bool is_open = false;
	CacheMemory cache;
	float sample_value = 0.0f;

public:
	TimelineConstantAudioReader(int width, int height,
	                            int fps_num, int fps_den,
	                            int sample_rate, int channels,
	                            int64_t length_frames, float fill_sample)
		: sample_value(fill_sample) {
		info.has_video = true;
		info.has_audio = true;
		info.width = width;
		info.height = height;
		info.fps = Fraction(fps_num, fps_den);
		info.video_length = length_frames;
		info.duration = static_cast<float>(length_frames / info.fps.ToDouble());
		info.sample_rate = sample_rate;
		info.channels = channels;
		info.channel_layout = LAYOUT_STEREO;
		info.audio_stream_index = 0;
	}

	openshot::CacheBase* GetCache() override { return &cache; }
	bool IsOpen() override { return is_open; }
	std::string Name() override { return "TimelineConstantAudioReader"; }
	void Open() override { is_open = true; }
	void Close() override { is_open = false; }

	std::shared_ptr<openshot::Frame> GetFrame(int64_t number) override {
		const int sample_count = Frame::GetSamplesPerFrame(number, info.fps, info.sample_rate, info.channels);
		auto frame = std::make_shared<Frame>(number, info.width, info.height, "#000000", sample_count, info.channels);
		std::vector<float> samples(sample_count, sample_value);
		for (int channel = 0; channel < info.channels; ++channel)
			frame->AddAudio(true, channel, 0, samples.data(), sample_count, 1.0f);
		return frame;
	}

	std::string Json() const override { return JsonValue().toStyledString(); }
	Json::Value JsonValue() const override {
		Json::Value root = ReaderBase::JsonValue();
		root["type"] = "TimelineConstantAudioReader";
		root["path"] = "";
		return root;
	}
	void SetJson(const std::string value) override { (void) value; }
	void SetJsonValue(const Json::Value root) override { ReaderBase::SetJsonValue(root); }
};

class TimelineFailFirstOpenReader : public ReaderBase {
private:
	bool is_open = false;
	bool fail_next_open = false;
	CacheMemory cache;
	QColor color;

public:
	TimelineFailFirstOpenReader(int width,
	                            int height,
	                            int fps_num,
	                            int fps_den,
	                            int64_t length_frames,
	                            const QColor& fill_color)
		: color(fill_color) {
		info.has_video = true;
		info.has_audio = false;
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
	std::string Name() override { return "TimelineFailFirstOpenReader"; }
	void FailNextOpen() { fail_next_open = true; }
	void Open() override {
		if (fail_next_open) {
			fail_next_open = false;
			throw InvalidFile("synthetic first open failure", "");
		}
		is_open = true;
	}
	void Close() override { is_open = false; }

	std::shared_ptr<openshot::Frame> GetFrame(int64_t number) override {
		if (!is_open)
			throw ReaderClosed("synthetic reader is closed");
		auto frame = std::make_shared<Frame>(number, info.width, info.height, "#00000000");
		frame->GetImage()->fill(color);
		return frame;
	}

	std::string Json() const override { return JsonValue().toStyledString(); }
	Json::Value JsonValue() const override {
		Json::Value root = ReaderBase::JsonValue();
		root["type"] = "TimelineFailFirstOpenReader";
		root["path"] = "";
		return root;
	}
	void SetJson(const std::string value) override { (void) value; }
	void SetJsonValue(const Json::Value root) override { ReaderBase::SetJsonValue(root); }
};

static double expected_equal_power_gain(int64_t frame_number, int64_t start_frame, int64_t end_frame, bool fades_in) {
	constexpr double kHalfPi = 1.57079632679489661923;
	if (end_frame <= start_frame)
		return 1.0;
	const double span = static_cast<double>(end_frame - start_frame);
	double t = static_cast<double>(frame_number - start_frame) / span;
	if (t < 0.0)
		t = 0.0;
	else if (t > 1.0)
		t = 1.0;
	return fades_in ? std::sin(t * kHalfPi) : std::cos(t * kHalfPi);
}

TEST_CASE("Timeline honors Mask fade_audio_hint with equal-power overlapping audio", "[libopenshot][timeline][audio][transition]") {
	const Fraction fps(30, 1);
	const int sample_rate = 48000;
	const int channels = 2;
	const int64_t length_frames = 90;
	const int64_t overlap_start_frame = 31;
	const int64_t overlap_end_frame = 60;

	Timeline t(320, 180, fps, sample_rate, channels, LAYOUT_STEREO);

	TimelineConstantAudioReader bottom_reader(320, 180, fps.num, fps.den, sample_rate, channels, length_frames, 1.0f);
	TimelineConstantAudioReader top_reader(320, 180, fps.num, fps.den, sample_rate, channels, length_frames, 1.0f);

	Clip bottom_clip;
	bottom_clip.Reader(&bottom_reader);
	bottom_clip.Layer(0);
	bottom_clip.Position(0.0);
	bottom_clip.Start(0.0);
	bottom_clip.End(2.0);
	bottom_clip.channel_filter = Keyframe(0.0);

	Clip top_clip;
	top_clip.Reader(&top_reader);
	top_clip.Layer(0);
	top_clip.Position(1.0);
	top_clip.Start(0.0);
	top_clip.End(2.0);
	top_clip.channel_filter = Keyframe(1.0);

	TimelineTrackingMaskReader mask_reader(fps.num, fps.den, overlap_end_frame - overlap_start_frame + 1);
	Mask transition;
	transition.Reader(&mask_reader);
	transition.Layer(0);
	transition.Position(1.0);
	transition.Start(0.0);
	transition.End(1.0);
	transition.brightness = Keyframe();
	transition.brightness.AddPoint(1, 1.0, BEZIER);
	transition.brightness.AddPoint(overlap_end_frame - overlap_start_frame + 1, -1.0, BEZIER);
	transition.contrast = Keyframe(3.0);

	t.AddClip(&bottom_clip);
	t.AddClip(&top_clip);
	t.AddEffect(&transition);
	t.Open();

	SECTION("disabled hint keeps raw overlap audio") {
		transition.fade_audio_hint = false;
		auto frame = t.GetFrame(45);
		const int last_sample = frame->GetAudioSamplesCount() - 1;
		CHECK(frame->GetAudioSamples(0)[0] == Approx(1.0).margin(0.0001));
		CHECK(frame->GetAudioSamples(1)[0] == Approx(1.0).margin(0.0001));
		CHECK(frame->GetAudioSamples(0)[last_sample] == Approx(1.0).margin(0.0001));
		CHECK(frame->GetAudioSamples(1)[last_sample] == Approx(1.0).margin(0.0001));
	}

	SECTION("enabled hint fades bottom out and top in") {
		transition.fade_audio_hint = true;

		auto start_frame = t.GetFrame(overlap_start_frame);
		CHECK(start_frame->GetAudioSamples(0)[0] == Approx(1.0).margin(0.0001));
		CHECK(start_frame->GetAudioSamples(1)[0] == Approx(0.0).margin(0.0001));

		auto middle_frame = t.GetFrame(45);
		const int middle_last_sample = middle_frame->GetAudioSamplesCount() - 1;
		const double expected_prev_bottom = expected_equal_power_gain(44, overlap_start_frame, overlap_end_frame, false);
		const double expected_prev_top = expected_equal_power_gain(44, overlap_start_frame, overlap_end_frame, true);
		const double expected_bottom = expected_equal_power_gain(45, overlap_start_frame, overlap_end_frame, false);
		const double expected_top = expected_equal_power_gain(45, overlap_start_frame, overlap_end_frame, true);
		CHECK(middle_frame->GetAudioSamples(0)[0] == Approx(expected_prev_bottom).margin(0.0002));
		CHECK(middle_frame->GetAudioSamples(1)[0] == Approx(expected_prev_top).margin(0.0002));
		CHECK(middle_frame->GetAudioSamples(0)[middle_last_sample] == Approx(expected_bottom).margin(0.002));
		CHECK(middle_frame->GetAudioSamples(1)[middle_last_sample] == Approx(expected_top).margin(0.002));

		auto end_frame = t.GetFrame(overlap_end_frame);
		const int end_last_sample = end_frame->GetAudioSamplesCount() - 1;
		CHECK(end_frame->GetAudioSamples(0)[end_last_sample] == Approx(0.0).margin(0.002));
		CHECK(end_frame->GetAudioSamples(1)[end_last_sample] == Approx(1.0).margin(0.002));
	}

	SECTION("reversed brightness does not affect geometry-based fade directions") {
		transition.fade_audio_hint = true;
		transition.brightness = Keyframe();
		transition.brightness.AddPoint(1, -1.0, BEZIER);
		transition.brightness.AddPoint(overlap_end_frame - overlap_start_frame + 1, 1.0, BEZIER);

		auto start_frame = t.GetFrame(overlap_start_frame);
		CHECK(start_frame->GetAudioSamples(0)[0] == Approx(1.0).margin(0.0001));
		CHECK(start_frame->GetAudioSamples(1)[0] == Approx(0.0).margin(0.0001));

		auto middle_frame = t.GetFrame(45);
		const int middle_last_sample = middle_frame->GetAudioSamplesCount() - 1;
		const double expected_prev_bottom = expected_equal_power_gain(44, overlap_start_frame, overlap_end_frame, false);
		const double expected_prev_top = expected_equal_power_gain(44, overlap_start_frame, overlap_end_frame, true);
		const double expected_bottom = expected_equal_power_gain(45, overlap_start_frame, overlap_end_frame, false);
		const double expected_top = expected_equal_power_gain(45, overlap_start_frame, overlap_end_frame, true);
		CHECK(middle_frame->GetAudioSamples(0)[0] == Approx(expected_prev_bottom).margin(0.0002));
		CHECK(middle_frame->GetAudioSamples(1)[0] == Approx(expected_prev_top).margin(0.0002));
		CHECK(middle_frame->GetAudioSamples(0)[middle_last_sample] == Approx(expected_bottom).margin(0.002));
		CHECK(middle_frame->GetAudioSamples(1)[middle_last_sample] == Approx(expected_top).margin(0.002));

		auto end_frame = t.GetFrame(overlap_end_frame);
		const int end_last_sample = end_frame->GetAudioSamplesCount() - 1;
		CHECK(end_frame->GetAudioSamples(0)[end_last_sample] == Approx(0.0).margin(0.002));
		CHECK(end_frame->GetAudioSamples(1)[end_last_sample] == Approx(1.0).margin(0.002));
	}

	t.Close();
}

TEST_CASE("Timeline uses transition edge proximity for single-clip fade audio", "[libopenshot][timeline][audio][transition][single]") {
	const Fraction fps(30, 1);
	const int sample_rate = 48000;
	const int channels = 2;
	const int64_t length_frames = 90;

	Timeline t(320, 180, fps, sample_rate, channels, LAYOUT_STEREO);

	TimelineConstantAudioReader clip_reader(320, 180, fps.num, fps.den, sample_rate, channels, length_frames, 1.0f);
	Clip clip;
	clip.Reader(&clip_reader);
	clip.Layer(0);
	clip.Position(1.0);
	clip.Start(0.0);
	clip.End(2.0);

	TimelineTrackingMaskReader mask_reader(fps.num, fps.den, 30);
	Mask transition;
	transition.Reader(&mask_reader);
	transition.Layer(0);
	transition.Start(0.0);
	transition.End(1.0);
	transition.fade_audio_hint = true;
	transition.brightness = Keyframe(0.0);
	transition.contrast = Keyframe(0.0);

	t.AddClip(&clip);
	t.AddEffect(&transition);
	t.Open();

	SECTION("left edge proximity fades in") {
		transition.Position(1.0);
		auto start_frame = t.GetFrame(31);
		auto end_frame = t.GetFrame(60);
		const int end_last_sample = end_frame->GetAudioSamplesCount() - 1;
		CHECK(start_frame->GetAudioSamples(0)[0] == Approx(0.0).margin(0.0001));
		CHECK(end_frame->GetAudioSamples(0)[end_last_sample] == Approx(1.0).margin(0.002));
	}

	SECTION("right edge proximity fades out") {
		transition.Position(2.0);
		auto start_frame = t.GetFrame(61);
		auto end_frame = t.GetFrame(90);
		const int end_last_sample = end_frame->GetAudioSamplesCount() - 1;
		CHECK(start_frame->GetAudioSamples(0)[0] == Approx(1.0).margin(0.0001));
		CHECK(end_frame->GetAudioSamples(0)[end_last_sample] == Approx(0.0).margin(0.002));
	}

	SECTION("equal distance defaults to fade in") {
		transition.Position(1.5);
		transition.End(1.0);
		auto start_frame = t.GetFrame(46);
		auto end_frame = t.GetFrame(75);
		const int end_last_sample = end_frame->GetAudioSamplesCount() - 1;
		CHECK(start_frame->GetAudioSamples(0)[0] == Approx(0.0).margin(0.0001));
		CHECK(end_frame->GetAudioSamples(0)[end_last_sample] == Approx(1.0).margin(0.002));
	}

	t.Close();
}

TEST_CASE( "constructor", "[libopenshot][timeline]" )
{
	Fraction fps(30000,1000);
	Timeline t1(640, 480, fps, 44100, 2, LAYOUT_STEREO);

	// Check values
	CHECK(t1.info.width == 640);
	CHECK(t1.info.height == 480);

	Timeline t2(300, 240, fps, 44100, 2, LAYOUT_STEREO);

	// Check values
	CHECK(t2.info.width == 300);
	CHECK(t2.info.height == 240);
}

TEST_CASE( "project constructor invalid path message", "[libopenshot][timeline]" )
{
	const std::string invalid_path = "/tmp/__openshot_missing_test_project__.osp";
	try {
		Timeline t(invalid_path, true);
		FAIL("Expected InvalidFile for missing timeline project path");
	} catch (const InvalidFile& e) {
		const std::string message = e.what();
		CHECK(message.find("Timeline project file could not be opened.") != std::string::npos);
		CHECK(message.find(invalid_path) != std::string::npos);
	}
}

TEST_CASE( "Set Json and clear clips", "[libopenshot][timeline]" )
{
	Fraction fps(30000,1000);
	Timeline t(640, 480, fps, 44100, 2, LAYOUT_STEREO);

	// Large ugly JSON project (4 clips + 3 transitions)
	std::stringstream project_json;
	project_json << "{\"id\":\"CQA0YW6I2Q\",\"fps\":{\"num\":30,\"den\":1},\"display_ratio\":{\"num\":16,\"den\":9},\"pixel_ratio\":{\"num\":1,\"den\":1},\"width\":1280,\"height\":720,\"sample_rate\":48000,\"channels\":2,\"channel_layout\":3,\"settings\":{},\"clips\":[{\"alpha\":{\"Points\":[{\"co\":{\"X\":1,\"Y\":1},\"handle_left\":{\"X\":0.5,\"Y\":1},\"handle_right\":{\"X\":0.5,\"Y\":0},\"handle_type\":0,\"interpolation\":0}]},\"anchor\":0,\"channel_filter\":{\"Points\":[{\"co\":{\"X\":1,\"Y\":-1},\"handle_left\":{\"X\":0.5,\"Y\":1},\"handle_right\":{\"X\":0.5,\"Y\":0},\"handle_type\":0,\"interpolation\":0}]},\"channel_mapping\":{\"Points\":[{\"co\":{\"X\":1,\"Y\":-1},\"handle_left\":{\"X\":0.5,\"Y\":1},\"handle_right\":{\"X\":0.5,\"Y\":0},\"handle_type\":0,\"interpolation\":0}]},\"display\":0,\"duration\":51.9466667175293,\"effects\":[],\"end\":10.666666666666666,\"gravity\":4,\"has_audio\":{\"Points\":[{\"co\":{\"X\":1,\"Y\":-1},\"handle_left\":{\"X\":0.5,\"Y\":1},\"handle_right\":{\"X\":0.5,\"Y\":0},\"handle_type\":0,\"interpolation\":0}]},\"has_video\":{\"Points\":[{\"co\":{\"X\":1,\"Y\":-1},\"handle_left\":{\"X\":0.5,\"Y\":1},\"handle_right\":{\"X\":0.5,\"Y\":0},\"handle_type\":0,\"interpolation\":0}]},\"id\":\"QHESI4ZW0E\",\"layer\":5000000,\"location_x\":{\"Points\":[{\"co\":{\"X\":1,\"Y\":0},\"handle_left\":{\"X\":0.5,\"Y\":1},\"handle_right\":{\"X\":0.5,\"Y\":0},\"handle_type\":0,\"interpolation\":0}]},\"location_y\":{\"Points\":[{\"co\":{\"X\":1,\"Y\":0},\"handle_left\":{\"X\":0.5,\"Y\":1},\"handle_right\":{\"X\":0.5,\"Y\":0},\"handle_type\":0,\"interpolation\":0}]},\"mixing\":0,\"origin_x\":{\"Points\":[{\"co\":{\"X\":1,\"Y\":0.5},\"handle_left\":{\"X\":0.5,\"Y\":1},\"handle_right\":{\"X\":0.5,\"Y\":0},\"handle_type\":0,\"interpolation\":0}]},\"origin_y\":{\"Points\":[{\"co\":{\"X\":1,\"Y\":0.5},\"handle_left\":{\"X\":0.5,\"Y\":1},\"handle_right\":{\"X\":0.5,\"Y\":0},\"handle_type\":0,\"interpolation\":0}]},\"parentObjectId\":\"\",\"perspective_c1_x\":{\"Points\":[{\"co\":{\"X\":1,\"Y\":-1},\"handle_left\":{\"X\":0.5,\"Y\":1},\"handle_right\":{\"X\":0.5,\"Y\":0},\"handle_type\":0,\"interpolation\":0}]},\"perspective_c1_y\":{\"Points\":[{\"co\":{\"X\":1,\"Y\":-1},\"handle_left\":{\"X\":0.5,\"Y\":1},\"handle_right\":{\"X\":0.5,\"Y\":0},\"handle_type\":0,\"interpolation\":0}]},\"perspective_c2_x\":{\"Points\":[{\"co\":{\"X\":1,\"Y\":-1},\"handle_left\":{\"X\":0.5,\"Y\":1},\"handle_right\":{\"X\":0.5,\"Y\":0},\"handle_type\":0,\"interpolation\":0}]},\"perspective_c2_y\":{\"Points\":[{\"co\":{\"X\":1,\"Y\":-1},\"handle_left\":{\"X\":0.5,\"Y\":1},\"handle_right\":{\"X\":0.5,\"Y\":0},\"handle_type\":0,\"interpolation\":0}]},\"perspective_c3_x\":{\"Points\":[{\"co\":{\"X\":1,\"Y\":-1},\"handle_left\":{\"X\":0.5,\"Y\":1},\"handle_right\":{\"X\":0.5,\"Y\":0},\"handle_type\":0,\"interpolation\":0}]},\"perspective_c3_y\":{\"Points\":[{\"co\":{\"X\":1,\"Y\":-1},\"handle_left\":{\"X\":0.5,\"Y\":1},\"handle_right\":{\"X\":0.5,\"Y\":0},\"handle_type\":0,\"interpolation\":0}]},\"perspective_c4_x\":{\"Points\":[{\"co\":{\"X\":1,\"Y\":-1},\"handle_left\":{\"X\":0.5,\"Y\":1},\"handle_right\":{\"X\":0.5,\"Y\":0},\"handle_type\":0,\"interpolation\":0}]},\"perspective_c4_y\":{\"Points\":[{\"co\":{\"X\":1,\"Y\":-1},\"handle_left\":{\"X\":0.5,\"Y\":1},\"handle_right\":{\"X\":0.5,\"Y\":0},\"handle_type\":0,\"interpolation\":0}]},\"position\":0,\"reader\":{\"acodec\":\"aac\",\"audio_bit_rate\":126694,\"audio_stream_index\":1,\"audio_timebase\":{\"den\":48000,\"num\":1},\"channel_layout\":3,\"channels\":2,\"display_ratio\":{\"den\":9,\"num\":16},\"duration\":51.9466667175293,\"file_size\":\"7608204\",\"fps\":{\"den\":1,\"num\":24},\"has_audio\":true,\"has_single_image\":false,\"has_video\":true,\"height\":720,\"interlaced_frame\":false,\"metadata\":{\"artist\":\"Durian Open Movie Team\",\"compatible_brands\":\"isomiso2avc1mp41\",\"copyright\":\"(c) copyright Blender Foundation | durian.blender.org\",\"creation_time\":\"1970-01-01T00:00:00.000000Z\",\"description\":\"Trailer for the Sintel open movie project\",\"encoder\":\"Lavf52.62.0\",\"handler_name\":\"SoundHandler\",\"language\":\"und\",\"major_brand\":\"isom\",\"minor_version\":\"512\",\"title\":\"Sintel Trailer\"},\"path\":\"" << TEST_MEDIA_PATH << "sintel_trailer-720p.mp4\",\"pixel_format\":0,\"pixel_ratio\":{\"den\":1,\"num\":1},\"sample_rate\":48000,\"top_field_first\":true,\"type\":\"FFmpegReader\",\"vcodec\":\"h264\",\"video_bit_rate\":145725,\"video_length\":\"1253\",\"video_stream_index\":0,\"video_timebase\":{\"den\":24,\"num\":1},\"width\":1280},\"rotation\":{\"Points\":[{\"co\":{\"X\":1,\"Y\":0},\"handle_left\":{\"X\":0.5,\"Y\":1},\"handle_right\":{\"X\":0.5,\"Y\":0},\"handle_type\":0,\"interpolation\":0}]},\"scale\":1,\"scale_x\":{\"Points\":[{\"co\":{\"X\":1,\"Y\":1},\"handle_left\":{\"X\":0.5,\"Y\":1},\"handle_right\":{\"X\":0.5,\"Y\":0},\"handle_type\":0,\"interpolation\":0}]},\"scale_y\":{\"Points\":[{\"co\":{\"X\":1,\"Y\":1},\"handle_left\":{\"X\":0.5,\"Y\":1},\"handle_right\":{\"X\":0.5,\"Y\":0},\"handle_type\":0,\"interpolation\":0}]},\"shear_x\":{\"Points\":[{\"co\":{\"X\":1,\"Y\":0},\"handle_left\":{\"X\":0.5,\"Y\":1},\"handle_right\":{\"X\":0.5,\"Y\":0},\"handle_type\":0,\"interpolation\":0}]},\"shear_y\":{\"Points\":[{\"co\":{\"X\":1,\"Y\":0},\"handle_left\":{\"X\":0.5,\"Y\":1},\"handle_right\":{\"X\":0.5,\"Y\":0},\"handle_type\":0,\"interpolation\":0}]},\"start\":0,\"time\":{\"Points\":[{\"co\":{\"X\":1,\"Y\":1},\"handle_left\":{\"X\":0.5,\"Y\":1},\"handle_right\":{\"X\":0.5,\"Y\":0},\"handle_type\":0,\"interpolation\":0}]},\"volume\":{\"Points\":[{\"co\":{\"X\":1,\"Y\":1},\"handle_left\":{\"X\":0.5,\"Y\":1},\"handle_right\":{\"X\":0.5,\"Y\":0},\"handle_type\":0,\"interpolation\":0}]},\"wave_color\":{\"alpha\":{\"Points\":[{\"co\":{\"X\":1,\"Y\":255},\"handle_left\":{\"X\":0.5,\"Y\":1},\"handle_right\":{\"X\":0.5,\"Y\":0},\"handle_type\":0,\"interpolation\":0}]},\"blue\":{\"Points\":[{\"co\":{\"X\":1,\"Y\":255},\"handle_left\":{\"X\":0.5,\"Y\":1},\"handle_right\":{\"X\":0.5,\"Y\":0},\"handle_type\":0,\"interpolation\":0}]},\"green\":{\"Points\":[{\"co\":{\"X\":1,\"Y\":123},\"handle_left\":{\"X\":0.5,\"Y\":1},\"handle_right\":{\"X\":0.5,\"Y\":0},\"handle_type\":0,\"interpolation\":0}]},\"red\":{\"Points\":[{\"co\":{\"X\":1,\"Y\":0},\"handle_left\":{\"X\":0.5,\"Y\":1},\"handle_right\":{\"X\":0.5,\"Y\":0},\"handle_type\":0,\"interpolation\":0}]}},\"waveform\":false,\"file_id\":\"XL7T80Y9R1\",\"title\":\"sintel_trailer-720p.mp4\",\"image\":\"@assets/thumbnail/XL7T80Y9R1.png\"},{\"alpha\":{\"Points\":[{\"co\":{\"X\":1,\"Y\":1},\"handle_left\":{\"X\":0.5,\"Y\":1},\"handle_right\":{\"X\":0.5,\"Y\":0},\"handle_type\":0,\"interpolation\":0}]},\"anchor\":0,\"channel_filter\":{\"Points\":[{\"co\":{\"X\":1,\"Y\":-1},\"handle_left\":{\"X\":0.5,\"Y\":1},\"handle_right\":{\"X\":0.5,\"Y\":0},\"handle_type\":0,\"interpolation\":0}]},\"channel_mapping\":{\"Points\":[{\"co\":{\"X\":1,\"Y\":-1},\"handle_left\":{\"X\":0.5,\"Y\":1},\"handle_right\":{\"X\":0.5,\"Y\":0},\"handle_type\":0,\"interpolation\":0}]},\"display\":0,\"duration\":51.9466667175293,\"effects\":[],\"end\":20.866666666666667,\"gravity\":4,\"has_audio\":{\"Points\":[{\"co\":{\"X\":1,\"Y\":-1},\"handle_left\":{\"X\":0.5,\"Y\":1},\"handle_right\":{\"X\":0.5,\"Y\":0},\"handle_type\":0,\"interpolation\":0}]},\"has_video\":{\"Points\":[{\"co\":{\"X\":1,\"Y\":-1},\"handle_left\":{\"X\":0.5,\"Y\":1},\"handle_right\":{\"X\":0.5,\"Y\":0},\"handle_type\":0,\"interpolation\":0}]},\"layer\":5000000,\"location_x\":{\"Points\":[{\"co\":{\"X\":1,\"Y\":0},\"handle_left\":{\"X\":0.5,\"Y\":1},\"handle_right\":{\"X\":0.5,\"Y\":0},\"handle_type\":0,\"interpolation\":0}]},\"location_y\":{\"Points\":[{\"co\":{\"X\":1,\"Y\":0},\"handle_left\":{\"X\":0.5,\"Y\":1},\"handle_right\":{\"X\":0.5,\"Y\":0},\"handle_type\":0,\"interpolation\":0}]},\"mixing\":0,\"origin_x\":{\"Points\":[{\"co\":{\"X\":1,\"Y\":0.5},\"handle_left\":{\"X\":0.5,\"Y\":1},\"handle_right\":{\"X\":0.5,\"Y\":0},\"handle_type\":0,\"interpolation\":0}]},\"origin_y\":{\"Points\":[{\"co\":{\"X\":1,\"Y\":0.5},\"handle_left\":{\"X\":0.5,\"Y\":1},\"handle_right\":{\"X\":0.5,\"Y\":0},\"handle_type\":0,\"interpolation\":0}]},\"parentObjectId\":\"\",\"perspective_c1_x\":{\"Points\":[{\"co\":{\"X\":1,\"Y\":-1},\"handle_left\":{\"X\":0.5,\"Y\":1},\"handle_right\":{\"X\":0.5,\"Y\":0},\"handle_type\":0,\"interpolation\":0}]},\"perspective_c1_y\":{\"Points\":[{\"co\":{\"X\":1,\"Y\":-1},\"handle_left\":{\"X\":0.5,\"Y\":1},\"handle_right\":{\"X\":0.5,\"Y\":0},\"handle_type\":0,\"interpolation\":0}]},\"perspective_c2_x\":{\"Points\":[{\"co\":{\"X\":1,\"Y\":-1},\"handle_left\":{\"X\":0.5,\"Y\":1},\"handle_right\":{\"X\":0.5,\"Y\":0},\"handle_type\":0,\"interpolation\":0}]},\"perspective_c2_y\":{\"Points\":[{\"co\":{\"X\":1,\"Y\":-1},\"handle_left\":{\"X\":0.5,\"Y\":1},\"handle_right\":{\"X\":0.5,\"Y\":0},\"handle_type\":0,\"interpolation\":0}]},\"perspective_c3_x\":{\"Points\":[{\"co\":{\"X\":1,\"Y\":-1},\"handle_left\":{\"X\":0.5,\"Y\":1},\"handle_right\":{\"X\":0.5,\"Y\":0},\"handle_type\":0,\"interpolation\":0}]},\"perspective_c3_y\":{\"Points\":[{\"co\":{\"X\":1,\"Y\":-1},\"handle_left\":{\"X\":0.5,\"Y\":1},\"handle_right\":{\"X\":0.5,\"Y\":0},\"handle_type\":0,\"interpolation\":0}]},\"perspective_c4_x\":{\"Points\":[{\"co\":{\"X\":1,\"Y\":-1},\"handle_left\":{\"X\":0.5,\"Y\":1},\"handle_right\":{\"X\":0.5,\"Y\":0},\"handle_type\":0,\"interpolation\":0}]},\"perspective_c4_y\":{\"Points\":[{\"co\":{\"X\":1,\"Y\":-1},\"handle_left\":{\"X\":0.5,\"Y\":1},\"handle_right\":{\"X\":0.5,\"Y\":0},\"handle_type\":0,\"interpolation\":0}]},\"position\":5.7,\"reader\":{\"acodec\":\"aac\",\"audio_bit_rate\":126694,\"audio_stream_index\":1,\"audio_timebase\":{\"den\":48000,\"num\":1},\"channel_layout\":3,\"channels\":2,\"display_ratio\":{\"den\":9,\"num\":16},\"duration\":51.9466667175293,\"file_size\":\"7608204\",\"fps\":{\"den\":1,\"num\":24},\"has_audio\":true,\"has_single_image\":false,\"has_video\":true,\"height\":720,\"interlaced_frame\":false,\"metadata\":{\"artist\":\"Durian Open Movie Team\",\"compatible_brands\":\"isomiso2avc1mp41\",\"copyright\":\"(c) copyright Blender Foundation | durian.blender.org\",\"creation_time\":\"1970-01-01T00:00:00.000000Z\",\"description\":\"Trailer for the Sintel open movie project\",\"encoder\":\"Lavf52.62.0\",\"handler_name\":\"SoundHandler\",\"language\":\"und\",\"major_brand\":\"isom\",\"minor_version\":\"512\",\"title\":\"Sintel Trailer\"},\"path\":\"" << TEST_MEDIA_PATH << "sintel_trailer-720p.mp4\",\"pixel_format\":0,\"pixel_ratio\":{\"den\":1,\"num\":1},\"sample_rate\":48000,\"top_field_first\":true,\"type\":\"FFmpegReader\",\"vcodec\":\"h264\",\"video_bit_rate\":145725,\"video_length\":\"1253\",\"video_stream_index\":0,\"video_timebase\":{\"den\":24,\"num\":1},\"width\":1280},\"rotation\":{\"Points\":[{\"co\":{\"X\":1,\"Y\":0},\"handle_left\":{\"X\":0.5,\"Y\":1},\"handle_right\":{\"X\":0.5,\"Y\":0},\"handle_type\":0,\"interpolation\":0}]},\"scale\":1,\"scale_x\":{\"Points\":[{\"co\":{\"X\":1,\"Y\":1},\"handle_left\":{\"X\":0.5,\"Y\":1},\"handle_right\":{\"X\":0.5,\"Y\":0},\"handle_type\":0,\"interpolation\":0}]},\"scale_y\":{\"Points\":[{\"co\":{\"X\":1,\"Y\":1},\"handle_left\":{\"X\":0.5,\"Y\":1},\"handle_right\":{\"X\":0.5,\"Y\":0},\"handle_type\":0,\"interpolation\":0}]},\"shear_x\":{\"Points\":[{\"co\":{\"X\":1,\"Y\":0},\"handle_left\":{\"X\":0.5,\"Y\":1},\"handle_right\":{\"X\":0.5,\"Y\":0},\"handle_type\":0,\"interpolation\":0}]},\"shear_y\":{\"Points\":[{\"co\":{\"X\":1,\"Y\":0},\"handle_left\":{\"X\":0.5,\"Y\":1},\"handle_right\":{\"X\":0.5,\"Y\":0},\"handle_type\":0,\"interpolation\":0}]},\"start\":10.666666666666666,\"time\":{\"Points\":[{\"co\":{\"X\":1,\"Y\":1},\"handle_left\":{\"X\":0.5,\"Y\":1},\"handle_right\":{\"X\":0.5,\"Y\":0},\"handle_type\":0,\"interpolation\":0}]},\"volume\":{\"Points\":[{\"co\":{\"X\":1,\"Y\":1},\"handle_left\":{\"X\":0.5,\"Y\":1},\"handle_right\":{\"X\":0.5,\"Y\":0},\"handle_type\":0,\"interpolation\":0}]},\"wave_color\":{\"alpha\":{\"Points\":[{\"co\":{\"X\":1,\"Y\":255},\"handle_left\":{\"X\":0.5,\"Y\":1},\"handle_right\":{\"X\":0.5,\"Y\":0},\"handle_type\":0,\"interpolation\":0}]},\"blue\":{\"Points\":[{\"co\":{\"X\":1,\"Y\":255},\"handle_left\":{\"X\":0.5,\"Y\":1},\"handle_right\":{\"X\":0.5,\"Y\":0},\"handle_type\":0,\"interpolation\":0}]},\"green\":{\"Points\":[{\"co\":{\"X\":1,\"Y\":123},\"handle_left\":{\"X\":0.5,\"Y\":1},\"handle_right\":{\"X\":0.5,\"Y\":0},\"handle_type\":0,\"interpolation\":0}]},\"red\":{\"Points\":[{\"co\":{\"X\":1,\"Y\":0},\"handle_left\":{\"X\":0.5,\"Y\":1},\"handle_right\":{\"X\":0.5,\"Y\":0},\"handle_type\":0,\"interpolation\":0}]}},\"waveform\":false,\"file_id\":\"XL7T80Y9R1\",\"title\":\"sintel_trailer-720p.mp4\",\"id\":\"KQK39ZFGJE\",\"image\":\"@assets/thumbnail/XL7T80Y9R1.png\"},{\"alpha\":{\"Points\":[{\"co\":{\"X\":1,\"Y\":1},\"handle_left\":{\"X\":0.5,\"Y\":1},\"handle_right\":{\"X\":0.5,\"Y\":0},\"handle_type\":0,\"interpolation\":0}]},\"anchor\":0,\"channel_filter\":{\"Points\":[{\"co\":{\"X\":1,\"Y\":-1},\"handle_left\":{\"X\":0.5,\"Y\":1},\"handle_right\":{\"X\":0.5,\"Y\":0},\"handle_type\":0,\"interpolation\":0}]},\"channel_mapping\":{\"Points\":[{\"co\":{\"X\":1,\"Y\":-1},\"handle_left\":{\"X\":0.5,\"Y\":1},\"handle_right\":{\"X\":0.5,\"Y\":0},\"handle_type\":0,\"interpolation\":0}]},\"display\":0,\"duration\":51.9466667175293,\"effects\":[],\"end\":29.566666666666666,\"gravity\":4,\"has_audio\":{\"Points\":[{\"co\":{\"X\":1,\"Y\":-1},\"handle_left\":{\"X\":0.5,\"Y\":1},\"handle_right\":{\"X\":0.5,\"Y\":0},\"handle_type\":0,\"interpolation\":0}]},\"has_video\":{\"Points\":[{\"co\":{\"X\":1,\"Y\":-1},\"handle_left\":{\"X\":0.5,\"Y\":1},\"handle_right\":{\"X\":0.5,\"Y\":0},\"handle_type\":0,\"interpolation\":0}]},\"layer\":5000000,\"location_x\":{\"Points\":[{\"co\":{\"X\":1,\"Y\":0},\"handle_left\":{\"X\":0.5,\"Y\":1},\"handle_right\":{\"X\":0.5,\"Y\":0},\"handle_type\":0,\"interpolation\":0}]},\"location_y\":{\"Points\":[{\"co\":{\"X\":1,\"Y\":0},\"handle_left\":{\"X\":0.5,\"Y\":1},\"handle_right\":{\"X\":0.5,\"Y\":0},\"handle_type\":0,\"interpolation\":0}]},\"mixing\":0,\"origin_x\":{\"Points\":[{\"co\":{\"X\":1,\"Y\":0.5},\"handle_left\":{\"X\":0.5,\"Y\":1},\"handle_right\":{\"X\":0.5,\"Y\":0},\"handle_type\":0,\"interpolation\":0}]},\"origin_y\":{\"Points\":[{\"co\":{\"X\":1,\"Y\":0.5},\"handle_left\":{\"X\":0.5,\"Y\":1},\"handle_right\":{\"X\":0.5,\"Y\":0},\"handle_type\":0,\"interpolation\":0}]},\"parentObjectId\":\"\",\"perspective_c1_x\":{\"Points\":[{\"co\":{\"X\":1,\"Y\":-1},\"handle_left\":{\"X\":0.5,\"Y\":1},\"handle_right\":{\"X\":0.5,\"Y\":0},\"handle_type\":0,\"interpolation\":0}]},\"perspective_c1_y\":{\"Points\":[{\"co\":{\"X\":1,\"Y\":-1},\"handle_left\":{\"X\":0.5,\"Y\":1},\"handle_right\":{\"X\":0.5,\"Y\":0},\"handle_type\":0,\"interpolation\":0}]},\"perspective_c2_x\":{\"Points\":[{\"co\":{\"X\":1,\"Y\":-1},\"handle_left\":{\"X\":0.5,\"Y\":1},\"handle_right\":{\"X\":0.5,\"Y\":0},\"handle_type\":0,\"interpolation\":0}]},\"perspective_c2_y\":{\"Points\":[{\"co\":{\"X\":1,\"Y\":-1},\"handle_left\":{\"X\":0.5,\"Y\":1},\"handle_right\":{\"X\":0.5,\"Y\":0},\"handle_type\":0,\"interpolation\":0}]},\"perspective_c3_x\":{\"Points\":[{\"co\":{\"X\":1,\"Y\":-1},\"handle_left\":{\"X\":0.5,\"Y\":1},\"handle_right\":{\"X\":0.5,\"Y\":0},\"handle_type\":0,\"interpolation\":0}]},\"perspective_c3_y\":{\"Points\":[{\"co\":{\"X\":1,\"Y\":-1},\"handle_left\":{\"X\":0.5,\"Y\":1},\"handle_right\":{\"X\":0.5,\"Y\":0},\"handle_type\":0,\"interpolation\":0}]},\"perspective_c4_x\":{\"Points\":[{\"co\":{\"X\":1,\"Y\":-1},\"handle_left\":{\"X\":0.5,\"Y\":1},\"handle_right\":{\"X\":0.5,\"Y\":0},\"handle_type\":0,\"interpolation\":0}]},\"perspective_c4_y\":{\"Points\":[{\"co\":{\"X\":1,\"Y\":-1},\"handle_left\":{\"X\":0.5,\"Y\":1},\"handle_right\":{\"X\":0.5,\"Y\":0},\"handle_type\":0,\"interpolation\":0}]},\"position\":12.3,\"reader\":{\"acodec\":\"aac\",\"audio_bit_rate\":126694,\"audio_stream_index\":1,\"audio_timebase\":{\"den\":48000,\"num\":1},\"channel_layout\":3,\"channels\":2,\"display_ratio\":{\"den\":9,\"num\":16},\"duration\":51.9466667175293,\"file_size\":\"7608204\",\"fps\":{\"den\":1,\"num\":24},\"has_audio\":true,\"has_single_image\":false,\"has_video\":true,\"height\":720,\"interlaced_frame\":false,\"metadata\":{\"artist\":\"Durian Open Movie Team\",\"compatible_brands\":\"isomiso2avc1mp41\",\"copyright\":\"(c) copyright Blender Foundation | durian.blender.org\",\"creation_time\":\"1970-01-01T00:00:00.000000Z\",\"description\":\"Trailer for the Sintel open movie project\",\"encoder\":\"Lavf52.62.0\",\"handler_name\":\"SoundHandler\",\"language\":\"und\",\"major_brand\":\"isom\",\"minor_version\":\"512\",\"title\":\"Sintel Trailer\"},\"path\":\"" << TEST_MEDIA_PATH << "sintel_trailer-720p.mp4\",\"pixel_format\":0,\"pixel_ratio\":{\"den\":1,\"num\":1},\"sample_rate\":48000,\"top_field_first\":true,\"type\":\"FFmpegReader\",\"vcodec\":\"h264\",\"video_bit_rate\":145725,\"video_length\":\"1253\",\"video_stream_index\":0,\"video_timebase\":{\"den\":24,\"num\":1},\"width\":1280},\"rotation\":{\"Points\":[{\"co\":{\"X\":1,\"Y\":0},\"handle_left\":{\"X\":0.5,\"Y\":1},\"handle_right\":{\"X\":0.5,\"Y\":0},\"handle_type\":0,\"interpolation\":0}]},\"scale\":1,\"scale_x\":{\"Points\":[{\"co\":{\"X\":1,\"Y\":1},\"handle_left\":{\"X\":0.5,\"Y\":1},\"handle_right\":{\"X\":0.5,\"Y\":0},\"handle_type\":0,\"interpolation\":0}]},\"scale_y\":{\"Points\":[{\"co\":{\"X\":1,\"Y\":1},\"handle_left\":{\"X\":0.5,\"Y\":1},\"handle_right\":{\"X\":0.5,\"Y\":0},\"handle_type\":0,\"interpolation\":0}]},\"shear_x\":{\"Points\":[{\"co\":{\"X\":1,\"Y\":0},\"handle_left\":{\"X\":0.5,\"Y\":1},\"handle_right\":{\"X\":0.5,\"Y\":0},\"handle_type\":0,\"interpolation\":0}]},\"shear_y\":{\"Points\":[{\"co\":{\"X\":1,\"Y\":0},\"handle_left\":{\"X\":0.5,\"Y\":1},\"handle_right\":{\"X\":0.5,\"Y\":0},\"handle_type\":0,\"interpolation\":0}]},\"start\":20.866666666666667,\"time\":{\"Points\":[{\"co\":{\"X\":1,\"Y\":1},\"handle_left\":{\"X\":0.5,\"Y\":1},\"handle_right\":{\"X\":0.5,\"Y\":0},\"handle_type\":0,\"interpolation\":0}]},\"volume\":{\"Points\":[{\"co\":{\"X\":1,\"Y\":1},\"handle_left\":{\"X\":0.5,\"Y\":1},\"handle_right\":{\"X\":0.5,\"Y\":0},\"handle_type\":0,\"interpolation\":0}]},\"wave_color\":{\"alpha\":{\"Points\":[{\"co\":{\"X\":1,\"Y\":255},\"handle_left\":{\"X\":0.5,\"Y\":1},\"handle_right\":{\"X\":0.5,\"Y\":0},\"handle_type\":0,\"interpolation\":0}]},\"blue\":{\"Points\":[{\"co\":{\"X\":1,\"Y\":255},\"handle_left\":{\"X\":0.5,\"Y\":1},\"handle_right\":{\"X\":0.5,\"Y\":0},\"handle_type\":0,\"interpolation\":0}]},\"green\":{\"Points\":[{\"co\":{\"X\":1,\"Y\":123},\"handle_left\":{\"X\":0.5,\"Y\":1},\"handle_right\":{\"X\":0.5,\"Y\":0},\"handle_type\":0,\"interpolation\":0}]},\"red\":{\"Points\":[{\"co\":{\"X\":1,\"Y\":0},\"handle_left\":{\"X\":0.5,\"Y\":1},\"handle_right\":{\"X\":0.5,\"Y\":0},\"handle_type\":0,\"interpolation\":0}]}},\"waveform\":false,\"file_id\":\"XL7T80Y9R1\",\"title\":\"sintel_trailer-720p.mp4\",\"id\":\"TMKI8CK7QQ\",\"image\":\"@assets/thumbnail/XL7T80Y9R1.png\"},{\"alpha\":{\"Points\":[{\"co\":{\"X\":1,\"Y\":0},\"handle_left\":{\"X\":0.5,\"Y\":1},\"handle_right\":{\"X\":0.5,\"Y\":0},\"handle_type\":0,\"interpolation\":0},{\"co\":{\"X\":91,\"Y\":1},\"handle_left\":{\"X\":0.5,\"Y\":1},\"handle_right\":{\"X\":0.5,\"Y\":0},\"handle_type\":0,\"interpolation\":0},{\"co\":{\"X\":541,\"Y\":1},\"handle_left\":{\"X\":0.5,\"Y\":1},\"handle_right\":{\"X\":0.5,\"Y\":0},\"handle_type\":0,\"interpolation\":0},{\"co\":{\"X\":631,\"Y\":0},\"handle_left\":{\"X\":0.5,\"Y\":1},\"handle_right\":{\"X\":0.5,\"Y\":0},\"handle_type\":0,\"interpolation\":0}]},\"anchor\":0,\"channel_filter\":{\"Points\":[{\"co\":{\"X\":1,\"Y\":-1},\"handle_left\":{\"X\":0.5,\"Y\":1},\"handle_right\":{\"X\":0.5,\"Y\":0},\"handle_type\":0,\"interpolation\":0}]},\"channel_mapping\":{\"Points\":[{\"co\":{\"X\":1,\"Y\":-1},\"handle_left\":{\"X\":0.5,\"Y\":1},\"handle_right\":{\"X\":0.5,\"Y\":0},\"handle_type\":0,\"interpolation\":0}]},\"display\":0,\"duration\":3600,\"effects\":[],\"end\":21,\"gravity\":4,\"has_audio\":{\"Points\":[{\"co\":{\"X\":1,\"Y\":-1},\"handle_left\":{\"X\":0.5,\"Y\":1},\"handle_right\":{\"X\":0.5,\"Y\":0},\"handle_type\":0,\"interpolation\":0}]},\"has_video\":{\"Points\":[{\"co\":{\"X\":1,\"Y\":-1},\"handle_left\":{\"X\":0.5,\"Y\":1},\"handle_right\":{\"X\":0.5,\"Y\":0},\"handle_type\":0,\"interpolation\":0}]},\"id\":\"2CQVCHPATF\",\"layer\":6000000,\"location_x\":{\"Points\":[{\"co\":{\"X\":1,\"Y\":0},\"handle_left\":{\"X\":0.5,\"Y\":1},\"handle_right\":{\"X\":0.5,\"Y\":0},\"handle_type\":0,\"interpolation\":0}]},\"location_y\":{\"Points\":[{\"co\":{\"X\":1,\"Y\":0},\"handle_left\":{\"X\":0.5,\"Y\":1},\"handle_right\":{\"X\":0.5,\"Y\":0},\"handle_type\":0,\"interpolation\":0}]},\"mixing\":0,\"origin_x\":{\"Points\":[{\"co\":{\"X\":1,\"Y\":0.5},\"handle_left\":{\"X\":0.5,\"Y\":1},\"handle_right\":{\"X\":0.5,\"Y\":0},\"handle_type\":0,\"interpolation\":0}]},\"origin_y\":{\"Points\":[{\"co\":{\"X\":1,\"Y\":0.5},\"handle_left\":{\"X\":0.5,\"Y\":1},\"handle_right\":{\"X\":0.5,\"Y\":0},\"handle_type\":0,\"interpolation\":0}]},\"parentObjectId\":\"\",\"perspective_c1_x\":{\"Points\":[{\"co\":{\"X\":1,\"Y\":-1},\"handle_left\":{\"X\":0.5,\"Y\":1},\"handle_right\":{\"X\":0.5,\"Y\":0},\"handle_type\":0,\"interpolation\":0}]},\"perspective_c1_y\":{\"Points\":[{\"co\":{\"X\":1,\"Y\":-1},\"handle_left\":{\"X\":0.5,\"Y\":1},\"handle_right\":{\"X\":0.5,\"Y\":0},\"handle_type\":0,\"interpolation\":0}]},\"perspective_c2_x\":{\"Points\":[{\"co\":{\"X\":1,\"Y\":-1},\"handle_left\":{\"X\":0.5,\"Y\":1},\"handle_right\":{\"X\":0.5,\"Y\":0},\"handle_type\":0,\"interpolation\":0}]},\"perspective_c2_y\":{\"Points\":[{\"co\":{\"X\":1,\"Y\":-1},\"handle_left\":{\"X\":0.5,\"Y\":1},\"handle_right\":{\"X\":0.5,\"Y\":0},\"handle_type\":0,\"interpolation\":0}]},\"perspective_c3_x\":{\"Points\":[{\"co\":{\"X\":1,\"Y\":-1},\"handle_left\":{\"X\":0.5,\"Y\":1},\"handle_right\":{\"X\":0.5,\"Y\":0},\"handle_type\":0,\"interpolation\":0}]},\"perspective_c3_y\":{\"Points\":[{\"co\":{\"X\":1,\"Y\":-1},\"handle_left\":{\"X\":0.5,\"Y\":1},\"handle_right\":{\"X\":0.5,\"Y\":0},\"handle_type\":0,\"interpolation\":0}]},\"perspective_c4_x\":{\"Points\":[{\"co\":{\"X\":1,\"Y\":-1},\"handle_left\":{\"X\":0.5,\"Y\":1},\"handle_right\":{\"X\":0.5,\"Y\":0},\"handle_type\":0,\"interpolation\":0}]},\"perspective_c4_y\":{\"Points\":[{\"co\":{\"X\":1,\"Y\":-1},\"handle_left\":{\"X\":0.5,\"Y\":1},\"handle_right\":{\"X\":0.5,\"Y\":0},\"handle_type\":0,\"interpolation\":0}]},\"position\":0,\"reader\":{\"acodec\":\"\",\"audio_bit_rate\":0,\"audio_stream_index\":-1,\"audio_timebase\":{\"den\":1,\"num\":1},\"channel_layout\":4,\"channels\":0,\"display_ratio\":{\"den\":2,\"num\":3},\"duration\":3600,\"file_size\":\"1382400\",\"fps\":{\"den\":1,\"num\":30},\"has_audio\":false,\"has_single_image\":true,\"has_video\":true,\"height\":480,\"interlaced_frame\":false,\"metadata\":{},\"path\":\"" << TEST_MEDIA_PATH << "front3.png\",\"pixel_format\":-1,\"pixel_ratio\":{\"den\":1,\"num\":1},\"sample_rate\":0,\"top_field_first\":true,\"type\":\"QtImageReader\",\"vcodec\":\"\",\"video_bit_rate\":0,\"video_length\":\"108000\",\"video_stream_index\":-1,\"video_timebase\":{\"den\":30,\"num\":1},\"width\":720},\"rotation\":{\"Points\":[{\"co\":{\"X\":1,\"Y\":0},\"handle_left\":{\"X\":0.5,\"Y\":1},\"handle_right\":{\"X\":0.5,\"Y\":0},\"handle_type\":0,\"interpolation\":0}]},\"scale\":1,\"scale_x\":{\"Points\":[{\"co\":{\"X\":1,\"Y\":1},\"handle_left\":{\"X\":0.5,\"Y\":1},\"handle_right\":{\"X\":0.5,\"Y\":0},\"handle_type\":0,\"interpolation\":0}]},\"scale_y\":{\"Points\":[{\"co\":{\"X\":1,\"Y\":1},\"handle_left\":{\"X\":0.5,\"Y\":1},\"handle_right\":{\"X\":0.5,\"Y\":0},\"handle_type\":0,\"interpolation\":0}]},\"shear_x\":{\"Points\":[{\"co\":{\"X\":1,\"Y\":0},\"handle_left\":{\"X\":0.5,\"Y\":1},\"handle_right\":{\"X\":0.5,\"Y\":0},\"handle_type\":0,\"interpolation\":0}]},\"shear_y\":{\"Points\":[{\"co\":{\"X\":1,\"Y\":0},\"handle_left\":{\"X\":0.5,\"Y\":1},\"handle_right\":{\"X\":0.5,\"Y\":0},\"handle_type\":0,\"interpolation\":0}]},\"start\":0,\"time\":{\"Points\":[{\"co\":{\"X\":1,\"Y\":1},\"handle_left\":{\"X\":0.5,\"Y\":1},\"handle_right\":{\"X\":0.5,\"Y\":0},\"handle_type\":0,\"interpolation\":0}]},\"volume\":{\"Points\":[{\"co\":{\"X\":1,\"Y\":1},\"handle_left\":{\"X\":0.5,\"Y\":1},\"handle_right\":{\"X\":0.5,\"Y\":0},\"handle_type\":0,\"interpolation\":0}]},\"wave_color\":{\"alpha\":{\"Points\":[{\"co\":{\"X\":1,\"Y\":255},\"handle_left\":{\"X\":0.5,\"Y\":1},\"handle_right\":{\"X\":0.5,\"Y\":0},\"handle_type\":0,\"interpolation\":0}]},\"blue\":{\"Points\":[{\"co\":{\"X\":1,\"Y\":255},\"handle_left\":{\"X\":0.5,\"Y\":1},\"handle_right\":{\"X\":0.5,\"Y\":0},\"handle_type\":0,\"interpolation\":0}]},\"green\":{\"Points\":[{\"co\":{\"X\":1,\"Y\":123},\"handle_left\":{\"X\":0.5,\"Y\":1},\"handle_right\":{\"X\":0.5,\"Y\":0},\"handle_type\":0,\"interpolation\":0}]},\"red\":{\"Points\":[{\"co\":{\"X\":1,\"Y\":0},\"handle_left\":{\"X\":0.5,\"Y\":1},\"handle_right\":{\"X\":0.5,\"Y\":0},\"handle_type\":0,\"interpolation\":0}]}},\"waveform\":false,\"file_id\":\"RY3OYWU7HK\",\"title\":\"front3.png\",\"image\":\"@assets/thumbnail/RY3OYWU7HK.png\"}],\"effects\":[{\"id\":\"335XHEZJNX\",\"layer\":5000000,\"title\":\"Transition\",\"type\":\"Mask\",\"position\":5.7,\"start\":0,\"end\":4.966666666666666,\"brightness\":{\"Points\":[{\"co\":{\"X\":1,\"Y\":1},\"handle_left\":{\"X\":0.5,\"Y\":1},\"handle_right\":{\"X\":0.5,\"Y\":0},\"handle_type\":0,\"interpolation\":0},{\"co\":{\"X\":150,\"Y\":-1},\"handle_left\":{\"X\":0.5,\"Y\":1},\"handle_right\":{\"X\":0.5,\"Y\":0},\"handle_type\":0,\"interpolation\":0}]},\"contrast\":{\"Points\":[{\"co\":{\"X\":1,\"Y\":3},\"handle_left\":{\"X\":0.5,\"Y\":1},\"handle_right\":{\"X\":0.5,\"Y\":0},\"handle_type\":0,\"interpolation\":0}]},\"reader\":{\"acodec\":\"\",\"audio_bit_rate\":0,\"audio_stream_index\":-1,\"audio_timebase\":{\"den\":1,\"num\":1},\"channel_layout\":4,\"channels\":0,\"display_ratio\":{\"den\":4,\"num\":5},\"duration\":3600,\"file_size\":\"5832000\",\"fps\":{\"den\":1,\"num\":30},\"has_audio\":false,\"has_single_image\":true,\"has_video\":true,\"height\":576,\"interlaced_frame\":false,\"metadata\":{},\"path\":\"" << TEST_MEDIA_PATH << "mask.png\",\"pixel_format\":-1,\"pixel_ratio\":{\"den\":1,\"num\":1},\"sample_rate\":0,\"top_field_first\":true,\"type\":\"QtImageReader\",\"vcodec\":\"\",\"video_bit_rate\":0,\"video_length\":\"108000\",\"video_stream_index\":-1,\"video_timebase\":{\"den\":30,\"num\":1},\"width\":720},\"replace_image\":false},{\"id\":\"QQECKBIYUP\",\"layer\":5000000,\"title\":\"Transition\",\"type\":\"Mask\",\"position\":12.3,\"start\":0,\"end\":3.6000000000000014,\"brightness\":{\"Points\":[{\"co\":{\"X\":1,\"Y\":1},\"handle_left\":{\"X\":0.5,\"Y\":1},\"handle_right\":{\"X\":0.5,\"Y\":0},\"handle_type\":0,\"interpolation\":0},{\"co\":{\"X\":109,\"Y\":-1},\"handle_left\":{\"X\":0.5,\"Y\":1},\"handle_right\":{\"X\":0.5,\"Y\":0},\"handle_type\":0,\"interpolation\":0}]},\"contrast\":{\"Points\":[{\"co\":{\"X\":1,\"Y\":3},\"handle_left\":{\"X\":0.5,\"Y\":1},\"handle_right\":{\"X\":0.5,\"Y\":0},\"handle_type\":0,\"interpolation\":0}]},\"reader\":{\"acodec\":\"\",\"audio_bit_rate\":0,\"audio_stream_index\":-1,\"audio_timebase\":{\"den\":1,\"num\":1},\"channel_layout\":4,\"channels\":0,\"display_ratio\":{\"den\":4,\"num\":5},\"duration\":3600,\"file_size\":\"5832000\",\"fps\":{\"den\":1,\"num\":30},\"has_audio\":false,\"has_single_image\":true,\"has_video\":true,\"height\":576,\"interlaced_frame\":false,\"metadata\":{},\"path\":\"" << TEST_MEDIA_PATH << "mask.png\",\"pixel_format\":-1,\"pixel_ratio\":{\"den\":1,\"num\":1},\"sample_rate\":0,\"top_field_first\":true,\"type\":\"QtImageReader\",\"vcodec\":\"\",\"video_bit_rate\":0,\"video_length\":\"108000\",\"video_stream_index\":-1,\"video_timebase\":{\"den\":30,\"num\":1},\"width\":720},\"replace_image\":false},{\"id\":\"YELU1J5KI8\",\"layer\":5000000,\"title\":\"Transition\",\"type\":\"Mask\",\"position\":17.7,\"start\":0,\"end\":3.3000000000000007,\"brightness\":{\"Points\":[{\"co\":{\"X\":1,\"Y\":1},\"handle_left\":{\"X\":0.5,\"Y\":1},\"handle_right\":{\"X\":0.5,\"Y\":0},\"handle_type\":0,\"interpolation\":0},{\"co\":{\"X\":100,\"Y\":-1},\"handle_left\":{\"X\":0.5,\"Y\":1},\"handle_right\":{\"X\":0.5,\"Y\":0},\"handle_type\":0,\"interpolation\":0}]},\"contrast\":{\"Points\":[{\"co\":{\"X\":1,\"Y\":3},\"handle_left\":{\"X\":0.5,\"Y\":1},\"handle_right\":{\"X\":0.5,\"Y\":0},\"handle_type\":0,\"interpolation\":0}]},\"reader\":{\"acodec\":\"\",\"audio_bit_rate\":0,\"audio_stream_index\":-1,\"audio_timebase\":{\"den\":1,\"num\":1},\"channel_layout\":4,\"channels\":0,\"display_ratio\":{\"den\":4,\"num\":5},\"duration\":3600,\"file_size\":\"5832000\",\"fps\":{\"den\":1,\"num\":30},\"has_audio\":false,\"has_single_image\":true,\"has_video\":true,\"height\":576,\"interlaced_frame\":false,\"metadata\":{},\"path\":\"" << TEST_MEDIA_PATH << "mask.png\",\"pixel_format\":-1,\"pixel_ratio\":{\"den\":1,\"num\":1},\"sample_rate\":0,\"top_field_first\":true,\"type\":\"QtImageReader\",\"vcodec\":\"\",\"video_bit_rate\":0,\"video_length\":\"108000\",\"video_stream_index\":-1,\"video_timebase\":{\"den\":30,\"num\":1},\"width\":720},\"replace_image\":false}],\"duration\":300,\"scale\":15,\"tick_pixels\":100,\"playhead_position\":0,\"profile\":\"HD 720p 30 fps\",\"layers\":[{\"id\":\"L1\",\"label\":\"\",\"number\":1000000,\"y\":0,\"lock\":false},{\"id\":\"L2\",\"label\":\"\",\"number\":2000000,\"y\":0,\"lock\":false},{\"id\":\"L3\",\"label\":\"\",\"number\":3000000,\"y\":0,\"lock\":false},{\"id\":\"L4\",\"label\":\"\",\"number\":4000000,\"y\":0,\"lock\":false},{\"id\":\"L5\",\"label\":\"\",\"number\":5000000,\"y\":0,\"lock\":false},{\"number\":6000000,\"y\":0,\"label\":\"\",\"lock\":false,\"id\":\"4U4NB9QVD2\"}],\"markers\":[],\"progress\":[],\"version\":{\"openshot-qt\":\"2.6.1-dev\",\"libopenshot\":\"0.2.7-dev\"}}";
	t.SetJson(project_json.str());

	// Count clips & effects
	CHECK(t.Clips().size() == 4);
	CHECK(t.Effects().size() == 3);

	// Clear timeline and clear allocated clips, effects, and frame mappers
	t.Clear();

	// Count clips & effects
	CHECK(t.Clips().size() == 0);
	CHECK(t.Effects().size() == 0);

	// Manually add clip object (not using SetJson)
	std::stringstream path;
	path << TEST_MEDIA_PATH << "test.mp4";
	Clip clip_video(path.str());
	t.AddClip(&clip_video);

	// Manually add effect object (not using SetJson)
	Negate effect_top;
	effect_top.Id("C");
	t.AddEffect(&effect_top);

	// Count clips & effects
	CHECK(t.Clips().size() == 1);
	CHECK(t.Effects().size() == 1);

	// Clear timeline
	t.Clear();

	// Count clips & effects
	CHECK(t.Clips().size() == 0);
	CHECK(t.Effects().size() == 0);
}

TEST_CASE("ReaderInfo constructor", "[libopenshot][timeline]")
{
	// Create a reader
	std::stringstream path;
	path << TEST_MEDIA_PATH << "test.mp4";
	Clip clip_video(path.str());
	clip_video.Open();
	const auto r1 = clip_video.Reader();

	// Configure a Timeline with the same parameters
	Timeline t1(r1->info);

	CHECK(r1->info.width == t1.info.width);
	CHECK(r1->info.height == t1.info.height);
	CHECK(r1->info.fps.num == t1.info.fps.num);
	CHECK(r1->info.fps.den == t1.info.fps.den);
	CHECK(r1->info.sample_rate == t1.info.sample_rate);
	CHECK(r1->info.channels == t1.info.channels);
	CHECK(r1->info.channel_layout == t1.info.channel_layout);
}

TEST_CASE("JSON diff insert keeps audio-only clip at stored timeline dimensions", "[libopenshot][timeline][json]")
{
	std::stringstream path;
	path << TEST_MEDIA_PATH << "piano.wav";

	Timeline t(1280, 720, Fraction(30, 1), 44100, 2, LAYOUT_STEREO);
	std::stringstream diff;
	diff << "[{\"type\":\"insert\",\"key\":[\"clips\"],\"value\":{"
		<< "\"id\":\"audio-copy\","
		<< "\"position\":0,"
		<< "\"start\":0,"
		<< "\"end\":1,"
		<< "\"duration\":1,"
		<< "\"layer\":1,"
		<< "\"reader\":{\"type\":\"FFmpegReader\","
		<< "\"path\":\"" << path.str() << "\","
		<< "\"has_audio\":true,"
		<< "\"has_video\":false,"
		<< "\"width\":1280,"
		<< "\"height\":720,"
		<< "\"fps\":{\"num\":30,\"den\":1},"
		<< "\"sample_rate\":44100,"
		<< "\"channels\":2,"
		<< "\"channel_layout\":3,"
		<< "\"duration\":1.0}"
		<< "}}]";

	t.ApplyJsonDiff(diff.str());

	std::list<Clip*> clips = t.Clips();
	REQUIRE(clips.size() == 1);
	Clip* clip = clips.front();
	CHECK(clip->ParentTimeline() == &t);
	CHECK(clip->Reader()->info.width == 1280);
	CHECK(clip->Reader()->info.height == 720);

	std::shared_ptr<Frame> frame = t.GetFrame(1);
	REQUIRE(frame);
	CHECK(frame->GetWidth() == 1280);
	CHECK(frame->GetHeight() == 720);
}

TEST_CASE( "width and height functions", "[libopenshot][timeline]" )
{
	Fraction fps(30000,1000);
	Timeline t1(640, 480, fps, 44100, 2, LAYOUT_STEREO);

	// Check values
	CHECK(t1.info.width == 640);
	CHECK(t1.info.height == 480);

	// Set width
	t1.info.width = 600;

	// Check values
	CHECK(t1.info.width == 600);
	CHECK(t1.info.height == 480);

	// Set height
	t1.info.height = 400;

	// Check values
	CHECK(t1.info.width == 600);
	CHECK(t1.info.height == 400);
}

TEST_CASE( "Framerate", "[libopenshot][timeline]" )
{
	Fraction fps(24,1);
	Timeline t1(640, 480, fps, 44100, 2, LAYOUT_STEREO);

	// Check values
	CHECK(t1.info.fps.ToFloat() == Approx(24.0f).margin(0.00001));
}

TEST_CASE( "two-track video", "[libopenshot][timeline]" )
{
	// Create a reader
	std::stringstream path;
	path << TEST_MEDIA_PATH << "test.mp4";
	Clip clip_video(path.str());
	clip_video.Layer(0);
	clip_video.Position(0.0);

	std::stringstream path_overlay;
	path_overlay << TEST_MEDIA_PATH << "front3.png";
	Clip clip_overlay(path_overlay.str());
	clip_overlay.Layer(1);
	clip_overlay.Position(0.05); // Delay the overlay by 0.05 seconds
	clip_overlay.End(0.5);	// Make the duration of the overlay 1/2 second

	// Create a timeline
	Timeline t(1280, 720, Fraction(30, 1), 44100, 2, LAYOUT_STEREO);

	// Add clips
	t.AddClip(&clip_video);
	t.AddClip(&clip_overlay);

	t.Open();

	std::shared_ptr<Frame> f = t.GetFrame(1);

	// Get the image data
	int pixel_row = 200;
	int pixel_index = 230 * 4; // pixel 230 (4 bytes per pixel)

	// Check image properties
	CHECK((int)f->GetPixels(pixel_row)[pixel_index] == Approx(21).margin(5));
	CHECK((int)f->GetPixels(pixel_row)[pixel_index + 1] == Approx(191).margin(5));
	CHECK((int)f->GetPixels(pixel_row)[pixel_index + 2] == Approx(0).margin(5));
	CHECK((int)f->GetPixels(pixel_row)[pixel_index + 3] == Approx(255).margin(5));

	f = t.GetFrame(2);

	// Check image properties
	CHECK((int)f->GetPixels(pixel_row)[pixel_index] == Approx(176).margin(5));
	CHECK((int)f->GetPixels(pixel_row)[pixel_index + 1] == Approx(0).margin(5));
	CHECK((int)f->GetPixels(pixel_row)[pixel_index + 2] == Approx(186).margin(5));
	CHECK((int)f->GetPixels(pixel_row)[pixel_index + 3] == Approx(255).margin(5));

	f = t.GetFrame(3);

	// Check image properties
	CHECK((int)f->GetPixels(pixel_row)[pixel_index] == Approx(23).margin(5));
	CHECK((int)f->GetPixels(pixel_row)[pixel_index + 1] == Approx(190).margin(5));
	CHECK((int)f->GetPixels(pixel_row)[pixel_index + 2] == Approx(0).margin(5));
	CHECK((int)f->GetPixels(pixel_row)[pixel_index + 3] == Approx(255).margin(5));

	f = t.GetFrame(24);

	// Check image properties
	CHECK((int)f->GetPixels(pixel_row)[pixel_index] == Approx(176).margin(5));
	CHECK((int)f->GetPixels(pixel_row)[pixel_index + 1] == Approx(0).margin(5));
	CHECK((int)f->GetPixels(pixel_row)[pixel_index + 2] == Approx(186).margin(5));
	CHECK((int)f->GetPixels(pixel_row)[pixel_index + 3] == Approx(255).margin(5));

	f = t.GetFrame(5);

	// Check image properties
	CHECK((int)f->GetPixels(pixel_row)[pixel_index] == Approx(23).margin(5));
	CHECK((int)f->GetPixels(pixel_row)[pixel_index + 1] == Approx(190).margin(5));
	CHECK((int)f->GetPixels(pixel_row)[pixel_index + 2] == Approx(0).margin(5));
	CHECK((int)f->GetPixels(pixel_row)[pixel_index + 3] == Approx(255).margin(5));

	f = t.GetFrame(25);

	// Check image properties
	CHECK((int)f->GetPixels(pixel_row)[pixel_index] == Approx(20).margin(5));
	CHECK((int)f->GetPixels(pixel_row)[pixel_index + 1] == Approx(190).margin(5));
	CHECK((int)f->GetPixels(pixel_row)[pixel_index + 2] == Approx(0).margin(5));
	CHECK((int)f->GetPixels(pixel_row)[pixel_index + 3] == Approx(255).margin(5));

	f = t.GetFrame(4);

	// Check image properties
	CHECK((int)f->GetPixels(pixel_row)[pixel_index] == Approx(176).margin(5));
	CHECK((int)f->GetPixels(pixel_row)[pixel_index + 1] == Approx(0).margin(5));
	CHECK((int)f->GetPixels(pixel_row)[pixel_index + 2] == Approx(186).margin(5));
	CHECK((int)f->GetPixels(pixel_row)[pixel_index + 3] == Approx(255).margin(5));

	t.Close();
}

TEST_CASE( "Clip order", "[libopenshot][timeline]" )
{
	// Create a timeline
	Timeline t(640, 480, Fraction(30, 1), 44100, 2, LAYOUT_STEREO);

	// Add some clips out of order
	std::stringstream path_top;
	path_top << TEST_MEDIA_PATH << "front3.png";
	Clip clip_top(path_top.str());
	clip_top.Layer(2);
	t.AddClip(&clip_top);

	std::stringstream path_middle;
	path_middle << TEST_MEDIA_PATH << "front.png";
	Clip clip_middle(path_middle.str());
	clip_middle.Layer(0);
	t.AddClip(&clip_middle);

	std::stringstream path_bottom;
	path_bottom << TEST_MEDIA_PATH << "back.png";
	Clip clip_bottom(path_bottom.str());
	clip_bottom.Layer(1);
	t.AddClip(&clip_bottom);

	t.Open();

	// Loop through Clips and check order (they should have been sorted into the correct order)
	// Bottom layer to top layer, then by position.
	std::list<Clip*> clips = t.Clips();
	int n = 0;
	for (auto clip : clips) {
		CHECK(clip->Layer() == n);
		++n;
	}

	// Add another clip
	std::stringstream path_middle1;
	path_middle1 << TEST_MEDIA_PATH << "interlaced.png";
	Clip clip_middle1(path_middle1.str());
	clip_middle1.Layer(1);
	clip_middle1.Position(0.5);
	t.AddClip(&clip_middle1);

	// Loop through clips again, and re-check order
	clips = t.Clips();
	n = 0;
	for (auto clip : clips) {
		switch (n) {
		case 0:
			CHECK(clip->Layer() == 0);
			break;
		case 1:
			CHECK(clip->Layer() == 1);
			CHECK(clip->Position() == Approx(0.0).margin(0.0001));
			break;
		case 2:
			CHECK(clip->Layer() == 1);
			CHECK(clip->Position() == Approx(0.5).margin(0.0001));
			break;
		case 3:
			CHECK(clip->Layer() == 2);
			break;
		}
		++n;
	}

	t.Close();
}

TEST_CASE( "TimelineBase", "[libopenshot][timeline]" )
{
	// Create a timeline
	Timeline t(640, 480, Fraction(30, 1), 44100, 2, LAYOUT_STEREO);

	// Add some clips out of order
	std::stringstream path;
	path << TEST_MEDIA_PATH << "front3.png";
	Clip clip1(path.str());
	clip1.Layer(1);
	t.AddClip(&clip1);

	Clip clip2(path.str());
	clip2.Layer(0);
	t.AddClip(&clip2);

	// Verify that the list of clips can be accessed
	// through the Clips() method of a TimelineBase*
	TimelineBase* base = &t;
	auto l = base->Clips();
	CHECK(l.size() == 2);
	auto find1 = std::find(l.begin(), l.end(), &clip1);
	auto find2 = std::find(l.begin(), l.end(), &clip2);
	CHECK(find1 != l.end());
	CHECK(find2 != l.end());
}


TEST_CASE( "Effect order", "[libopenshot][timeline]" )
{
	// Create a timeline
	Timeline t(640, 480, Fraction(30, 1), 44100, 2, LAYOUT_STEREO);

	// Add some effects out of order
	Negate effect_top;
	effect_top.Id("C");
	effect_top.Layer(2);
	t.AddEffect(&effect_top);

	Negate effect_middle;
	effect_middle.Id("A");
	effect_middle.Layer(0);
	t.AddEffect(&effect_middle);

	Negate effect_bottom;
	effect_bottom.Id("B");
	effect_bottom.Layer(1);
	t.AddEffect(&effect_bottom);

	t.Open();

	// Loop through effects and check order (they should have been sorted into the correct order)
	// Bottom layer to top layer, then by position, and then by order.
	std::list<EffectBase*> effects = t.Effects();
	int n = 0;
	for (auto effect : effects) {
		CHECK(effect->Layer() == n);
		CHECK(effect->Order() == 0);
		switch (n) {
		case 0:
			CHECK(effect->Id() == "A");
			break;
		case 1:
			CHECK(effect->Id() == "B");
			break;
		case 2:
			CHECK(effect->Id() == "C");
			break;
		}
		++n;
	}

	// Add some more effects out of order
	Negate effect_top1;
	effect_top1.Id("B-2");
	effect_top1.Layer(1);
	effect_top1.Position(0.5);
	effect_top1.Order(2);
	t.AddEffect(&effect_top1);

	Negate effect_middle1;
	effect_middle1.Id("B-3");
	effect_middle1.Layer(1);
	effect_middle1.Position(0.5);
	effect_middle1.Order(1);
	t.AddEffect(&effect_middle1);

	Negate effect_bottom1;
	effect_bottom1.Id("B-1");
	effect_bottom1.Layer(1);
	effect_bottom1.Position(0);
	effect_bottom1.Order(3);
	t.AddEffect(&effect_bottom1);


	// Loop through effects again, and re-check order
	effects = t.Effects();
	n = 0;
	for (auto effect : effects) {
		switch (n) {
		case 0:
			CHECK(effect->Layer() == 0);
			CHECK(effect->Id() == "A");
			CHECK(effect->Order() == 0);
			break;
		case 1:
			CHECK(effect->Layer() == 1);
			CHECK(effect->Id() == "B-1");
			CHECK(effect->Position() == Approx(0.0).margin(0.0001));
			CHECK(effect->Order() == 3);
			break;
		case 2:
			CHECK(effect->Layer() == 1);
			CHECK(effect->Id() == "B");
			CHECK(effect->Position() == Approx(0.0).margin(0.0001));
			CHECK(effect->Order() == 0);
			break;
		case 3:
			CHECK(effect->Layer() == 1);
			CHECK(effect->Id() == "B-2");
			CHECK(effect->Position() == Approx(0.5).margin(0.0001));
			CHECK(effect->Order() == 2);
			break;
		case 4:
			CHECK(effect->Layer() == 1);
			CHECK(effect->Id() == "B-3");
			CHECK(effect->Position() == Approx(0.5).margin(0.0001));
			CHECK(effect->Order() == 1);
			break;
		case 5:
			CHECK(effect->Layer() == 2);
			CHECK(effect->Id() == "C");
			CHECK(effect->Order() == 0);
			break;
		}
		++n;
	}

	t.Close();
}

TEST_CASE( "GetClip by id", "[libopenshot][timeline]" )
{
	Timeline t(640, 480, Fraction(30, 1), 44100, 2, LAYOUT_STEREO);

	std::stringstream path1;
	path1 << TEST_MEDIA_PATH << "interlaced.png";
	auto media_path1 = path1.str();

	std::stringstream path2;
	path2 << TEST_MEDIA_PATH << "front.png";
	auto media_path2 = path2.str();

	Clip clip1(media_path1);
	std::string clip1_id("CLIP00001");
	clip1.Id(clip1_id);
	clip1.Layer(1);

	Clip clip2(media_path2);
	std::string clip2_id("CLIP00002");
	clip2.Id(clip2_id);
	clip2.Layer(2);
	clip2.Waveform(true);

	t.AddClip(&clip1);
	t.AddClip(&clip2);

	// We explicitly want to get returned a Clip*, here
	Clip* matched = t.GetClip(clip1_id);
	CHECK(matched->Id() == clip1_id);
	CHECK(matched->Layer() == 1);

	Clip* matched2 = t.GetClip(clip2_id);
	CHECK(matched2->Id() == clip2_id);
	CHECK_FALSE(matched2->Layer() < 2);

	Clip* matched3 = t.GetClip("BAD_ID");
	CHECK(matched3 == nullptr);

	// Ensure we can access the Clip API interfaces after lookup
	CHECK_FALSE(matched->Waveform());
	CHECK(matched2->Waveform() == true);
}

TEST_CASE( "GetClipEffect by id", "[libopenshot][timeline]" )
{
	Timeline t(640, 480, Fraction(30, 1), 44100, 2, LAYOUT_STEREO);

	std::stringstream path1;
	path1 << TEST_MEDIA_PATH << "interlaced.png";
	auto media_path1 = path1.str();

	// Create a clip, nothing special
	Clip clip1(media_path1);
	std::string clip1_id("CLIP00001");
	clip1.Id(clip1_id);
	clip1.Layer(1);

	// Add a blur effect
	Keyframe horizontal_radius(5.0);
	Keyframe vertical_radius(5.0);
	Keyframe sigma(3.0);
	Keyframe iterations(3.0);
	Blur blur1(horizontal_radius, vertical_radius, sigma, iterations);
	std::string blur1_id("EFFECT00011");
	blur1.Id(blur1_id);
	clip1.AddEffect(&blur1);

	// A second clip, different layer
	Clip clip2(media_path1);
	std::string clip2_id("CLIP00002");
	clip2.Id(clip2_id);
	clip2.Layer(2);

	// Some effects for clip2
	Negate neg2;
	std::string neg2_id("EFFECT00021");
	neg2.Id(neg2_id);
	neg2.Layer(2);
	clip2.AddEffect(&neg2);
	Blur blur2(horizontal_radius, vertical_radius, sigma, iterations);
	std::string blur2_id("EFFECT00022");
	blur2.Id(blur2_id);
	blur2.Layer(2);
	clip2.AddEffect(&blur2);

	t.AddClip(&clip1);

	// Check that we can look up clip1's effect
	auto match1 = t.GetClipEffect("EFFECT00011");
	CHECK(match1->Id() == blur1_id);

	// clip2 hasn't been added yet, shouldn't be found
	match1 = t.GetClipEffect(blur2_id);
	CHECK(match1 == nullptr);

	t.AddClip(&clip2);

	// Check that blur2 can now be found via clip2
	match1 = t.GetClipEffect(blur2_id);
	CHECK(match1->Id() == blur2_id);
	CHECK(match1->Layer() == 2);
}

TEST_CASE( "Parent effect update preserves child effect id", "[libopenshot][timeline]" )
{
	Timeline t(640, 480, Fraction(30, 1), 44100, 2, LAYOUT_STEREO);

	std::stringstream path1;
	path1 << TEST_MEDIA_PATH << "interlaced.png";
	auto media_path1 = path1.str();

	Clip parent_clip(media_path1);
	parent_clip.Id("CLIP-PARENT");

	Clip child_clip(media_path1);
	child_clip.Id("CLIP-CHILD");

	Clip grandchild_clip(media_path1);
	grandchild_clip.Id("CLIP-GRANDCHILD");

	t.AddClip(&parent_clip);
	t.AddClip(&child_clip);
	t.AddClip(&grandchild_clip);

	Saturation parent_effect;
	parent_effect.Id("EFFECT-PARENT");
	parent_clip.AddEffect(&parent_effect);

	Saturation child_effect;
	child_effect.Id("EFFECT-CHILD");
	child_clip.AddEffect(&child_effect);

	Saturation grandchild_effect;
	grandchild_effect.Id("EFFECT-GRANDCHILD");
	grandchild_clip.AddEffect(&grandchild_effect);

	Json::Value child_json = child_effect.JsonValue();
	child_json["parent_effect_id"] = parent_effect.Id();
	child_effect.SetJsonValue(child_json);
	REQUIRE(t.GetClipEffect("EFFECT-CHILD") != nullptr);

	Json::Value grandchild_json = grandchild_effect.JsonValue();
	grandchild_json["parent_effect_id"] = child_effect.Id();
	grandchild_effect.SetJsonValue(grandchild_json);
	REQUIRE(t.GetClipEffect("EFFECT-GRANDCHILD") != nullptr);

	Json::Value parent_json = parent_effect.JsonValue();
	parent_json["order"] = 7;
	parent_json["saturation"] = Keyframe(2.25).JsonValue();
	parent_json["saturation_R"] = Keyframe(0.5).JsonValue();
	parent_effect.SetJsonValue(parent_json);

	REQUIRE(t.GetClipEffect("EFFECT-PARENT") != nullptr);
	REQUIRE(t.GetClipEffect("EFFECT-CHILD") != nullptr);
	REQUIRE(t.GetClipEffect("EFFECT-GRANDCHILD") != nullptr);
	CHECK(t.GetClipEffect("EFFECT-CHILD")->Id() == "EFFECT-CHILD");
	CHECK(t.GetClipEffect("EFFECT-GRANDCHILD")->Id() == "EFFECT-GRANDCHILD");
	CHECK(child_effect.Order() == 7);
	CHECK(grandchild_effect.Order() == 7);
	CHECK(child_effect.saturation.GetValue(1) == Approx(2.25));
	CHECK(child_effect.saturation_R.GetValue(1) == Approx(0.5));
	CHECK(grandchild_effect.saturation.GetValue(1) == Approx(2.25));
	CHECK(grandchild_effect.saturation_R.GetValue(1) == Approx(0.5));
	CHECK(openshot::stringToJson(child_effect.PropertiesJSON(1))["parent_effect_id"]["memo"].asString() == "EFFECT-PARENT");
	CHECK(openshot::stringToJson(grandchild_effect.PropertiesJSON(1))["parent_effect_id"]["memo"].asString() == "EFFECT-CHILD");
}

TEST_CASE( "GetEffect by id", "[libopenshot][timeline]" )
{
	Timeline t(640, 480, Fraction(30, 1), 44100, 2, LAYOUT_STEREO);

	// Create a timeline effect
	Keyframe horizontal_radius(5.0);
	Keyframe vertical_radius(5.0);
	Keyframe sigma(3.0);
	Keyframe iterations(3.0);
	Blur blur1(horizontal_radius, vertical_radius, sigma, iterations);
	std::string blur1_id("EFFECT00011");
	blur1.Id(blur1_id);
	blur1.Layer(1);
	t.AddEffect(&blur1);

	auto match1 = t.GetEffect(blur1_id);
	CHECK(match1->Id() == blur1_id);
	CHECK(match1->Layer() == 1);

	match1 = t.GetEffect("NOSUCHNAME");
	CHECK(match1 == nullptr);
}

TEST_CASE( "Effect: Blur", "[libopenshot][timeline]" )
{
	// Create a timeline
	Timeline t(640, 480, Fraction(30, 1), 44100, 2, LAYOUT_STEREO);

	std::stringstream path_top;
	path_top << TEST_MEDIA_PATH << "interlaced.png";
	Clip clip_top(path_top.str());
	clip_top.Layer(2);
	t.AddClip(&clip_top);

	// Add some effects out of order
	Keyframe horizontal_radius(5.0);
	Keyframe vertical_radius(5.0);
	Keyframe sigma(3.0);
	Keyframe iterations(3.0);
	Blur blur(horizontal_radius, vertical_radius, sigma, iterations);
	blur.Id("B");
	blur.Layer(2);
	t.AddEffect(&blur);

	// Open Timeline
	t.Open();

	// Get frame
	std::shared_ptr<Frame> f = t.GetFrame(1);

	REQUIRE(f != nullptr);
	CHECK(f->number == 1);

	// Close reader
	t.Close();
}

TEST_CASE("Global mask effect source FPS mode follows timeline FPS mapping", "[libopenshot][timeline][effect][mask][timing]") {
	Timeline t(320, 240, Fraction(30, 1), 44100, 2, LAYOUT_STEREO);

	DummyReader clip_reader(Fraction(30, 1), 320, 240, 44100, 2, 2.0f);
	Clip clip(&clip_reader);
	clip.Layer(0);
	clip.Position(0.0);
	clip.Start(0.0);
	clip.End(1.0);
	t.AddClip(&clip);

	Brightness effect(Keyframe(0.0), Keyframe(0.0));
	effect.Layer(0);
	effect.Position(0.0);
	effect.Start(0.0);
	effect.End(1.0);

	auto* tracking = new TimelineTrackingMaskReader(15, 1, 120);
	effect.MaskReader(tracking);

	Json::Value timing;
	timing["mask_time_mode"] = 1; // Source FPS
	timing["mask_loop_mode"] = 0; // Play Once
	timing["start"] = 0.0;
	timing["end"] = 1.0;
	effect.SetJsonValue(timing);

	t.AddEffect(&effect);
	t.Open();

	for (int64_t frame = 1; frame <= 5; ++frame) {
		auto out = t.GetFrame(frame);
		REQUIRE(out != nullptr);
	}

	const std::vector<int64_t> expected = {1, 2, 2, 3, 3};
	CHECK(tracking->requests == expected);

	t.Close();
}

TEST_CASE("Global mask effect start trims source without freezing playback", "[libopenshot][timeline][effect][mask][trim]") {
	Timeline t(320, 240, Fraction(30, 1), 44100, 2, LAYOUT_STEREO);

	DummyReader clip_reader(Fraction(30, 1), 320, 240, 44100, 2, 2.0f);
	Clip clip(&clip_reader);
	clip.Layer(0);
	clip.Position(0.0);
	clip.Start(0.0);
	clip.End(1.0);
	t.AddClip(&clip);

	Brightness effect(Keyframe(0.0), Keyframe(0.0));
	effect.Layer(0);
	effect.Position(0.0);
	effect.Start(1.0 / 15.0);
	effect.End(1.0);

	auto* tracking = new TimelineTrackingMaskReader(15, 1, 120);
	effect.MaskReader(tracking);

	Json::Value timing;
	timing["mask_time_mode"] = 1; // Source FPS
	timing["mask_loop_mode"] = 0; // Play Once
	timing["start"] = 1.0 / 15.0;
	timing["end"] = 1.0;
	effect.SetJsonValue(timing);

	t.AddEffect(&effect);
	t.Open();

	for (int64_t frame = 1; frame <= 5; ++frame) {
		auto out = t.GetFrame(frame);
		REQUIRE(out != nullptr);
	}

	const std::vector<int64_t> expected = {2, 3, 3, 4, 4};
	CHECK(tracking->requests == expected);

	t.Close();
}

TEST_CASE( "GetMaxFrame and GetMaxTime", "[libopenshot][timeline]" )
{
	// Create a timeline
	Timeline t(640, 480, Fraction(30, 1), 44100, 2, LAYOUT_STEREO);

	std::stringstream path1;
	path1 << TEST_MEDIA_PATH << "interlaced.png";
	Clip clip1(path1.str());
	clip1.Id("C1");
	clip1.Layer(1);
	clip1.Position(50);
	clip1.End(45);
	t.AddClip(&clip1);

	CHECK(t.GetMaxTime() == Approx(95.0).margin(0.001));
	CHECK(t.GetMaxFrame() == 95 * 30);

	Clip clip2(path1.str());
	clip2.Id("C2");
	clip2.Layer(2);
	clip2.Position(0);
	clip2.End(55);
	t.AddClip(&clip2);

	CHECK(t.GetMaxFrame() == 95 * 30);
	CHECK(t.GetMaxTime() == Approx(95.0).margin(0.001));

	clip1.Position(80);
	clip2.Position(100);
	CHECK(t.GetMaxFrame() == 155 * 30);
	CHECK(t.GetMaxTime() == Approx(155.0).margin(0.001));

	clip2.Start(20);
	CHECK(t.GetMaxFrame() == 135 * 30);
	CHECK(t.GetMaxTime() == Approx(135.0).margin(0.001));

	clip2.End(35);
	CHECK(t.GetMaxFrame() == 125 * 30);
	CHECK(t.GetMaxTime() == Approx(125.0).margin(0.001));

	t.RemoveClip(&clip1);
	CHECK(t.GetMaxFrame() == 115 * 30);
	CHECK(t.GetMaxTime() == Approx(115.0).margin(0.001));

	// Update Clip's basic properties with JSON Diff
	std::stringstream json_change1;
	json_change1 << "[{\"type\":\"update\",\"key\":[\"clips\",{\"id\":\"C2\"}],\"value\":{\"id\":\"C2\",\"layer\":4000000,\"position\":0.0,\"start\":0,\"end\":10},\"partial\":false}]";
	t.ApplyJsonDiff(json_change1.str());

	CHECK(t.GetMaxFrame() == 10 * 30);
	CHECK(t.GetMaxTime() == Approx(10.0).margin(0.001));

	// Insert NEW Clip with JSON Diff
	std::stringstream json_change2;
	json_change2 << "[{\"type\":\"insert\",\"key\":[\"clips\"],\"value\":{\"id\":\"C3\",\"layer\":4000000,\"position\":10.0,\"start\":0,\"end\":10,\"reader\":{\"acodec\":\"\",\"audio_bit_rate\":0,\"audio_stream_index\":-1,\"audio_timebase\":{\"den\":1,\"num\":1},\"channel_layout\":4,\"channels\":0,\"display_ratio\":{\"den\":1,\"num\":1},\"duration\":3600.0,\"file_size\":\"160000\",\"fps\":{\"den\":1,\"num\":30},\"has_audio\":false,\"has_single_image\":true,\"has_video\":true,\"height\":200,\"interlaced_frame\":false,\"metadata\":{},\"path\":\"" << path1.str() << "\",\"pixel_format\":-1,\"pixel_ratio\":{\"den\":1,\"num\":1},\"sample_rate\":0,\"top_field_first\":true,\"type\":\"QtImageReader\",\"vcodec\":\"\",\"video_bit_rate\":0,\"video_length\":\"108000\",\"video_stream_index\":-1,\"video_timebase\":{\"den\":30,\"num\":1},\"width\":200}},\"partial\":false}]";
	t.ApplyJsonDiff(json_change2.str());

	CHECK(t.GetMaxFrame() == 20 * 30);
	CHECK(t.GetMaxTime() == Approx(20.0).margin(0.001));
}

TEST_CASE( "GetMinFrame and GetMinTime", "[libopenshot][timeline]" )
{
    // Create a timeline
    Timeline t(640, 480, Fraction(30, 1), 44100, 2, LAYOUT_STEREO);

    std::stringstream path1;
    path1 << TEST_MEDIA_PATH << "interlaced.png";
    Clip clip1(path1.str());
    clip1.Id("C1");
    clip1.Layer(1);
    clip1.Position(50); // Start at 50 seconds
    clip1.End(45);      // Ends at 95 seconds
    t.AddClip(&clip1);

    CHECK(t.GetMinTime() == Approx(50.0).margin(0.001));
    CHECK(t.GetMinFrame() == (50 * 30) + 1);

    Clip clip2(path1.str());
    clip2.Id("C2");
    clip2.Layer(2);
    clip2.Position(0);  // Start at 0 seconds
    clip2.End(55);      // Ends at 55 seconds
    t.AddClip(&clip2);

    CHECK(t.GetMinTime() == Approx(0.0).margin(0.001));
    CHECK(t.GetMinFrame() == 1);

    clip1.Position(80); // Move clip1 to start at 80 seconds
    clip2.Position(100); // Move clip2 to start at 100 seconds
    CHECK(t.GetMinTime() == Approx(80.0).margin(0.001));
    CHECK(t.GetMinFrame() == (80 * 30) + 1);

    clip2.Position(20); // Adjust clip2 to start at 20 seconds
    CHECK(t.GetMinTime() == Approx(20.0).margin(0.001));
    CHECK(t.GetMinFrame() == (20 * 30) + 1);

    clip2.End(35); // Adjust clip2 to end at 35 seconds
    CHECK(t.GetMinTime() == Approx(20.0).margin(0.001));
    CHECK(t.GetMinFrame() == (20 * 30) + 1);

    t.RemoveClip(&clip1);
    CHECK(t.GetMinTime() == Approx(20.0).margin(0.001));
    CHECK(t.GetMinFrame() == (20 * 30) + 1);

    // Update Clip's basic properties with JSON Diff
    std::stringstream json_change1;
    json_change1 << "[{\"type\":\"update\",\"key\":[\"clips\",{\"id\":\"C2\"}],\"value\":{\"id\":\"C2\",\"layer\":4000000,\"position\":5.0,\"start\":0,\"end\":10},\"partial\":false}]";
    t.ApplyJsonDiff(json_change1.str());

    CHECK(t.GetMinTime() == Approx(5.0).margin(0.001));
    CHECK(t.GetMinFrame() == (5 * 30) + 1);

    // Insert NEW Clip with JSON Diff
    std::stringstream json_change2;
    json_change2 << "[{\"type\":\"insert\",\"key\":[\"clips\"],\"value\":{\"id\":\"C3\",\"layer\":4000000,\"position\":10.0,\"start\":0,\"end\":10,\"reader\":{\"acodec\":\"\",\"audio_bit_rate\":0,\"audio_stream_index\":-1,\"audio_timebase\":{\"den\":1,\"num\":1},\"channel_layout\":4,\"channels\":0,\"display_ratio\":{\"den\":1,\"num\":1},\"duration\":3600.0,\"file_size\":\"160000\",\"fps\":{\"den\":1,\"num\":30},\"has_audio\":false,\"has_single_image\":true,\"has_video\":true,\"height\":200,\"interlaced_frame\":false,\"metadata\":{},\"path\":\"" << path1.str() << "\",\"pixel_format\":-1,\"pixel_ratio\":{\"den\":1,\"num\":1},\"sample_rate\":0,\"top_field_first\":true,\"type\":\"QtImageReader\",\"vcodec\":\"\",\"video_bit_rate\":0,\"video_length\":\"108000\",\"video_stream_index\":-1,\"video_timebase\":{\"den\":30,\"num\":1},\"width\":200}},\"partial\":false}]";
    t.ApplyJsonDiff(json_change2.str());

    CHECK(t.GetMinTime() == Approx(5.0).margin(0.001));
    CHECK(t.GetMinFrame() == (5 * 30) + 1);
}

TEST_CASE( "GetMaxFrame with 24fps clip mapped to 30fps timeline", "[libopenshot][timeline]" )
{
	Timeline t(640, 480, Fraction(30, 1), 44100, 2, LAYOUT_STEREO);
	t.AutoMapClips(true);

	std::stringstream path;
	path << TEST_MEDIA_PATH << "sintel_trailer-720p.mp4";
	Clip clip(path.str());

	REQUIRE(clip.Reader()->info.fps.num == 24);
	REQUIRE(clip.Reader()->info.fps.den == 1);

	t.AddClip(&clip);

	REQUIRE(clip.Reader()->Name() == "FrameMapper");
	auto* mapper = static_cast<FrameMapper*>(clip.Reader());
	REQUIRE(mapper->info.fps.num == 30);
	REQUIRE(mapper->info.fps.den == 1);
	REQUIRE(mapper->info.video_length > 0);

	const int64_t timeline_max_frame = t.GetMaxFrame();
	const int64_t mapped_video_length = mapper->info.video_length;

	// Timeline max frame is computed from duration (seconds), while mapper length is
	// rounded frame count. They should stay aligned within one frame at this boundary.
	CHECK(timeline_max_frame >= mapped_video_length);
	CHECK((timeline_max_frame - mapped_video_length) <= 1);

	// Regression guard: fetching the mapped tail frame should not throw.
	t.Open();
	CHECK_NOTHROW(t.GetFrame(mapped_video_length));
	t.Close();
}

TEST_CASE( "Multi-threaded Timeline GetFrame", "[libopenshot][timeline]" )
{
	Timeline *t = new Timeline(1280, 720, Fraction(24, 1), 48000, 2, LAYOUT_STEREO);

	// Large ugly JSON project (4 clips + 3 transitions)
	std::stringstream project_json;
	project_json << "{\"id\":\"CQA0YW6I2Q\",\"fps\":{\"num\":30,\"den\":1},\"display_ratio\":{\"num\":16,\"den\":9},\"pixel_ratio\":{\"num\":1,\"den\":1},\"width\":1280,\"height\":720,\"sample_rate\":48000,\"channels\":2,\"channel_layout\":3,\"settings\":{},\"clips\":[{\"alpha\":{\"Points\":[{\"co\":{\"X\":1,\"Y\":1},\"handle_left\":{\"X\":0.5,\"Y\":1},\"handle_right\":{\"X\":0.5,\"Y\":0},\"handle_type\":0,\"interpolation\":0}]},\"anchor\":0,\"channel_filter\":{\"Points\":[{\"co\":{\"X\":1,\"Y\":-1},\"handle_left\":{\"X\":0.5,\"Y\":1},\"handle_right\":{\"X\":0.5,\"Y\":0},\"handle_type\":0,\"interpolation\":0}]},\"channel_mapping\":{\"Points\":[{\"co\":{\"X\":1,\"Y\":-1},\"handle_left\":{\"X\":0.5,\"Y\":1},\"handle_right\":{\"X\":0.5,\"Y\":0},\"handle_type\":0,\"interpolation\":0}]},\"display\":0,\"duration\":51.9466667175293,\"effects\":[],\"end\":10.666666666666666,\"gravity\":4,\"has_audio\":{\"Points\":[{\"co\":{\"X\":1,\"Y\":-1},\"handle_left\":{\"X\":0.5,\"Y\":1},\"handle_right\":{\"X\":0.5,\"Y\":0},\"handle_type\":0,\"interpolation\":0}]},\"has_video\":{\"Points\":[{\"co\":{\"X\":1,\"Y\":-1},\"handle_left\":{\"X\":0.5,\"Y\":1},\"handle_right\":{\"X\":0.5,\"Y\":0},\"handle_type\":0,\"interpolation\":0}]},\"id\":\"QHESI4ZW0E\",\"layer\":5000000,\"location_x\":{\"Points\":[{\"co\":{\"X\":1,\"Y\":0},\"handle_left\":{\"X\":0.5,\"Y\":1},\"handle_right\":{\"X\":0.5,\"Y\":0},\"handle_type\":0,\"interpolation\":0}]},\"location_y\":{\"Points\":[{\"co\":{\"X\":1,\"Y\":0},\"handle_left\":{\"X\":0.5,\"Y\":1},\"handle_right\":{\"X\":0.5,\"Y\":0},\"handle_type\":0,\"interpolation\":0}]},\"mixing\":0,\"origin_x\":{\"Points\":[{\"co\":{\"X\":1,\"Y\":0.5},\"handle_left\":{\"X\":0.5,\"Y\":1},\"handle_right\":{\"X\":0.5,\"Y\":0},\"handle_type\":0,\"interpolation\":0}]},\"origin_y\":{\"Points\":[{\"co\":{\"X\":1,\"Y\":0.5},\"handle_left\":{\"X\":0.5,\"Y\":1},\"handle_right\":{\"X\":0.5,\"Y\":0},\"handle_type\":0,\"interpolation\":0}]},\"parentObjectId\":\"\",\"perspective_c1_x\":{\"Points\":[{\"co\":{\"X\":1,\"Y\":-1},\"handle_left\":{\"X\":0.5,\"Y\":1},\"handle_right\":{\"X\":0.5,\"Y\":0},\"handle_type\":0,\"interpolation\":0}]},\"perspective_c1_y\":{\"Points\":[{\"co\":{\"X\":1,\"Y\":-1},\"handle_left\":{\"X\":0.5,\"Y\":1},\"handle_right\":{\"X\":0.5,\"Y\":0},\"handle_type\":0,\"interpolation\":0}]},\"perspective_c2_x\":{\"Points\":[{\"co\":{\"X\":1,\"Y\":-1},\"handle_left\":{\"X\":0.5,\"Y\":1},\"handle_right\":{\"X\":0.5,\"Y\":0},\"handle_type\":0,\"interpolation\":0}]},\"perspective_c2_y\":{\"Points\":[{\"co\":{\"X\":1,\"Y\":-1},\"handle_left\":{\"X\":0.5,\"Y\":1},\"handle_right\":{\"X\":0.5,\"Y\":0},\"handle_type\":0,\"interpolation\":0}]},\"perspective_c3_x\":{\"Points\":[{\"co\":{\"X\":1,\"Y\":-1},\"handle_left\":{\"X\":0.5,\"Y\":1},\"handle_right\":{\"X\":0.5,\"Y\":0},\"handle_type\":0,\"interpolation\":0}]},\"perspective_c3_y\":{\"Points\":[{\"co\":{\"X\":1,\"Y\":-1},\"handle_left\":{\"X\":0.5,\"Y\":1},\"handle_right\":{\"X\":0.5,\"Y\":0},\"handle_type\":0,\"interpolation\":0}]},\"perspective_c4_x\":{\"Points\":[{\"co\":{\"X\":1,\"Y\":-1},\"handle_left\":{\"X\":0.5,\"Y\":1},\"handle_right\":{\"X\":0.5,\"Y\":0},\"handle_type\":0,\"interpolation\":0}]},\"perspective_c4_y\":{\"Points\":[{\"co\":{\"X\":1,\"Y\":-1},\"handle_left\":{\"X\":0.5,\"Y\":1},\"handle_right\":{\"X\":0.5,\"Y\":0},\"handle_type\":0,\"interpolation\":0}]},\"position\":0,\"reader\":{\"acodec\":\"aac\",\"audio_bit_rate\":126694,\"audio_stream_index\":1,\"audio_timebase\":{\"den\":48000,\"num\":1},\"channel_layout\":3,\"channels\":2,\"display_ratio\":{\"den\":9,\"num\":16},\"duration\":51.9466667175293,\"file_size\":\"7608204\",\"fps\":{\"den\":1,\"num\":24},\"has_audio\":true,\"has_single_image\":false,\"has_video\":true,\"height\":720,\"interlaced_frame\":false,\"metadata\":{\"artist\":\"Durian Open Movie Team\",\"compatible_brands\":\"isomiso2avc1mp41\",\"copyright\":\"(c) copyright Blender Foundation | durian.blender.org\",\"creation_time\":\"1970-01-01T00:00:00.000000Z\",\"description\":\"Trailer for the Sintel open movie project\",\"encoder\":\"Lavf52.62.0\",\"handler_name\":\"SoundHandler\",\"language\":\"und\",\"major_brand\":\"isom\",\"minor_version\":\"512\",\"title\":\"Sintel Trailer\"},\"path\":\"" << TEST_MEDIA_PATH << "sintel_trailer-720p.mp4\",\"pixel_format\":0,\"pixel_ratio\":{\"den\":1,\"num\":1},\"sample_rate\":48000,\"top_field_first\":true,\"type\":\"FFmpegReader\",\"vcodec\":\"h264\",\"video_bit_rate\":145725,\"video_length\":\"1253\",\"video_stream_index\":0,\"video_timebase\":{\"den\":24,\"num\":1},\"width\":1280},\"rotation\":{\"Points\":[{\"co\":{\"X\":1,\"Y\":0},\"handle_left\":{\"X\":0.5,\"Y\":1},\"handle_right\":{\"X\":0.5,\"Y\":0},\"handle_type\":0,\"interpolation\":0}]},\"scale\":1,\"scale_x\":{\"Points\":[{\"co\":{\"X\":1,\"Y\":1},\"handle_left\":{\"X\":0.5,\"Y\":1},\"handle_right\":{\"X\":0.5,\"Y\":0},\"handle_type\":0,\"interpolation\":0}]},\"scale_y\":{\"Points\":[{\"co\":{\"X\":1,\"Y\":1},\"handle_left\":{\"X\":0.5,\"Y\":1},\"handle_right\":{\"X\":0.5,\"Y\":0},\"handle_type\":0,\"interpolation\":0}]},\"shear_x\":{\"Points\":[{\"co\":{\"X\":1,\"Y\":0},\"handle_left\":{\"X\":0.5,\"Y\":1},\"handle_right\":{\"X\":0.5,\"Y\":0},\"handle_type\":0,\"interpolation\":0}]},\"shear_y\":{\"Points\":[{\"co\":{\"X\":1,\"Y\":0},\"handle_left\":{\"X\":0.5,\"Y\":1},\"handle_right\":{\"X\":0.5,\"Y\":0},\"handle_type\":0,\"interpolation\":0}]},\"start\":0,\"time\":{\"Points\":[{\"co\":{\"X\":1,\"Y\":1},\"handle_left\":{\"X\":0.5,\"Y\":1},\"handle_right\":{\"X\":0.5,\"Y\":0},\"handle_type\":0,\"interpolation\":0}]},\"volume\":{\"Points\":[{\"co\":{\"X\":1,\"Y\":1},\"handle_left\":{\"X\":0.5,\"Y\":1},\"handle_right\":{\"X\":0.5,\"Y\":0},\"handle_type\":0,\"interpolation\":0}]},\"wave_color\":{\"alpha\":{\"Points\":[{\"co\":{\"X\":1,\"Y\":255},\"handle_left\":{\"X\":0.5,\"Y\":1},\"handle_right\":{\"X\":0.5,\"Y\":0},\"handle_type\":0,\"interpolation\":0}]},\"blue\":{\"Points\":[{\"co\":{\"X\":1,\"Y\":255},\"handle_left\":{\"X\":0.5,\"Y\":1},\"handle_right\":{\"X\":0.5,\"Y\":0},\"handle_type\":0,\"interpolation\":0}]},\"green\":{\"Points\":[{\"co\":{\"X\":1,\"Y\":123},\"handle_left\":{\"X\":0.5,\"Y\":1},\"handle_right\":{\"X\":0.5,\"Y\":0},\"handle_type\":0,\"interpolation\":0}]},\"red\":{\"Points\":[{\"co\":{\"X\":1,\"Y\":0},\"handle_left\":{\"X\":0.5,\"Y\":1},\"handle_right\":{\"X\":0.5,\"Y\":0},\"handle_type\":0,\"interpolation\":0}]}},\"waveform\":false,\"file_id\":\"XL7T80Y9R1\",\"title\":\"sintel_trailer-720p.mp4\",\"image\":\"@assets/thumbnail/XL7T80Y9R1.png\"},{\"alpha\":{\"Points\":[{\"co\":{\"X\":1,\"Y\":1},\"handle_left\":{\"X\":0.5,\"Y\":1},\"handle_right\":{\"X\":0.5,\"Y\":0},\"handle_type\":0,\"interpolation\":0}]},\"anchor\":0,\"channel_filter\":{\"Points\":[{\"co\":{\"X\":1,\"Y\":-1},\"handle_left\":{\"X\":0.5,\"Y\":1},\"handle_right\":{\"X\":0.5,\"Y\":0},\"handle_type\":0,\"interpolation\":0}]},\"channel_mapping\":{\"Points\":[{\"co\":{\"X\":1,\"Y\":-1},\"handle_left\":{\"X\":0.5,\"Y\":1},\"handle_right\":{\"X\":0.5,\"Y\":0},\"handle_type\":0,\"interpolation\":0}]},\"display\":0,\"duration\":51.9466667175293,\"effects\":[],\"end\":20.866666666666667,\"gravity\":4,\"has_audio\":{\"Points\":[{\"co\":{\"X\":1,\"Y\":-1},\"handle_left\":{\"X\":0.5,\"Y\":1},\"handle_right\":{\"X\":0.5,\"Y\":0},\"handle_type\":0,\"interpolation\":0}]},\"has_video\":{\"Points\":[{\"co\":{\"X\":1,\"Y\":-1},\"handle_left\":{\"X\":0.5,\"Y\":1},\"handle_right\":{\"X\":0.5,\"Y\":0},\"handle_type\":0,\"interpolation\":0}]},\"layer\":5000000,\"location_x\":{\"Points\":[{\"co\":{\"X\":1,\"Y\":0},\"handle_left\":{\"X\":0.5,\"Y\":1},\"handle_right\":{\"X\":0.5,\"Y\":0},\"handle_type\":0,\"interpolation\":0}]},\"location_y\":{\"Points\":[{\"co\":{\"X\":1,\"Y\":0},\"handle_left\":{\"X\":0.5,\"Y\":1},\"handle_right\":{\"X\":0.5,\"Y\":0},\"handle_type\":0,\"interpolation\":0}]},\"mixing\":0,\"origin_x\":{\"Points\":[{\"co\":{\"X\":1,\"Y\":0.5},\"handle_left\":{\"X\":0.5,\"Y\":1},\"handle_right\":{\"X\":0.5,\"Y\":0},\"handle_type\":0,\"interpolation\":0}]},\"origin_y\":{\"Points\":[{\"co\":{\"X\":1,\"Y\":0.5},\"handle_left\":{\"X\":0.5,\"Y\":1},\"handle_right\":{\"X\":0.5,\"Y\":0},\"handle_type\":0,\"interpolation\":0}]},\"parentObjectId\":\"\",\"perspective_c1_x\":{\"Points\":[{\"co\":{\"X\":1,\"Y\":-1},\"handle_left\":{\"X\":0.5,\"Y\":1},\"handle_right\":{\"X\":0.5,\"Y\":0},\"handle_type\":0,\"interpolation\":0}]},\"perspective_c1_y\":{\"Points\":[{\"co\":{\"X\":1,\"Y\":-1},\"handle_left\":{\"X\":0.5,\"Y\":1},\"handle_right\":{\"X\":0.5,\"Y\":0},\"handle_type\":0,\"interpolation\":0}]},\"perspective_c2_x\":{\"Points\":[{\"co\":{\"X\":1,\"Y\":-1},\"handle_left\":{\"X\":0.5,\"Y\":1},\"handle_right\":{\"X\":0.5,\"Y\":0},\"handle_type\":0,\"interpolation\":0}]},\"perspective_c2_y\":{\"Points\":[{\"co\":{\"X\":1,\"Y\":-1},\"handle_left\":{\"X\":0.5,\"Y\":1},\"handle_right\":{\"X\":0.5,\"Y\":0},\"handle_type\":0,\"interpolation\":0}]},\"perspective_c3_x\":{\"Points\":[{\"co\":{\"X\":1,\"Y\":-1},\"handle_left\":{\"X\":0.5,\"Y\":1},\"handle_right\":{\"X\":0.5,\"Y\":0},\"handle_type\":0,\"interpolation\":0}]},\"perspective_c3_y\":{\"Points\":[{\"co\":{\"X\":1,\"Y\":-1},\"handle_left\":{\"X\":0.5,\"Y\":1},\"handle_right\":{\"X\":0.5,\"Y\":0},\"handle_type\":0,\"interpolation\":0}]},\"perspective_c4_x\":{\"Points\":[{\"co\":{\"X\":1,\"Y\":-1},\"handle_left\":{\"X\":0.5,\"Y\":1},\"handle_right\":{\"X\":0.5,\"Y\":0},\"handle_type\":0,\"interpolation\":0}]},\"perspective_c4_y\":{\"Points\":[{\"co\":{\"X\":1,\"Y\":-1},\"handle_left\":{\"X\":0.5,\"Y\":1},\"handle_right\":{\"X\":0.5,\"Y\":0},\"handle_type\":0,\"interpolation\":0}]},\"position\":5.7,\"reader\":{\"acodec\":\"aac\",\"audio_bit_rate\":126694,\"audio_stream_index\":1,\"audio_timebase\":{\"den\":48000,\"num\":1},\"channel_layout\":3,\"channels\":2,\"display_ratio\":{\"den\":9,\"num\":16},\"duration\":51.9466667175293,\"file_size\":\"7608204\",\"fps\":{\"den\":1,\"num\":24},\"has_audio\":true,\"has_single_image\":false,\"has_video\":true,\"height\":720,\"interlaced_frame\":false,\"metadata\":{\"artist\":\"Durian Open Movie Team\",\"compatible_brands\":\"isomiso2avc1mp41\",\"copyright\":\"(c) copyright Blender Foundation | durian.blender.org\",\"creation_time\":\"1970-01-01T00:00:00.000000Z\",\"description\":\"Trailer for the Sintel open movie project\",\"encoder\":\"Lavf52.62.0\",\"handler_name\":\"SoundHandler\",\"language\":\"und\",\"major_brand\":\"isom\",\"minor_version\":\"512\",\"title\":\"Sintel Trailer\"},\"path\":\"" << TEST_MEDIA_PATH << "sintel_trailer-720p.mp4\",\"pixel_format\":0,\"pixel_ratio\":{\"den\":1,\"num\":1},\"sample_rate\":48000,\"top_field_first\":true,\"type\":\"FFmpegReader\",\"vcodec\":\"h264\",\"video_bit_rate\":145725,\"video_length\":\"1253\",\"video_stream_index\":0,\"video_timebase\":{\"den\":24,\"num\":1},\"width\":1280},\"rotation\":{\"Points\":[{\"co\":{\"X\":1,\"Y\":0},\"handle_left\":{\"X\":0.5,\"Y\":1},\"handle_right\":{\"X\":0.5,\"Y\":0},\"handle_type\":0,\"interpolation\":0}]},\"scale\":1,\"scale_x\":{\"Points\":[{\"co\":{\"X\":1,\"Y\":1},\"handle_left\":{\"X\":0.5,\"Y\":1},\"handle_right\":{\"X\":0.5,\"Y\":0},\"handle_type\":0,\"interpolation\":0}]},\"scale_y\":{\"Points\":[{\"co\":{\"X\":1,\"Y\":1},\"handle_left\":{\"X\":0.5,\"Y\":1},\"handle_right\":{\"X\":0.5,\"Y\":0},\"handle_type\":0,\"interpolation\":0}]},\"shear_x\":{\"Points\":[{\"co\":{\"X\":1,\"Y\":0},\"handle_left\":{\"X\":0.5,\"Y\":1},\"handle_right\":{\"X\":0.5,\"Y\":0},\"handle_type\":0,\"interpolation\":0}]},\"shear_y\":{\"Points\":[{\"co\":{\"X\":1,\"Y\":0},\"handle_left\":{\"X\":0.5,\"Y\":1},\"handle_right\":{\"X\":0.5,\"Y\":0},\"handle_type\":0,\"interpolation\":0}]},\"start\":10.666666666666666,\"time\":{\"Points\":[{\"co\":{\"X\":1,\"Y\":1},\"handle_left\":{\"X\":0.5,\"Y\":1},\"handle_right\":{\"X\":0.5,\"Y\":0},\"handle_type\":0,\"interpolation\":0}]},\"volume\":{\"Points\":[{\"co\":{\"X\":1,\"Y\":1},\"handle_left\":{\"X\":0.5,\"Y\":1},\"handle_right\":{\"X\":0.5,\"Y\":0},\"handle_type\":0,\"interpolation\":0}]},\"wave_color\":{\"alpha\":{\"Points\":[{\"co\":{\"X\":1,\"Y\":255},\"handle_left\":{\"X\":0.5,\"Y\":1},\"handle_right\":{\"X\":0.5,\"Y\":0},\"handle_type\":0,\"interpolation\":0}]},\"blue\":{\"Points\":[{\"co\":{\"X\":1,\"Y\":255},\"handle_left\":{\"X\":0.5,\"Y\":1},\"handle_right\":{\"X\":0.5,\"Y\":0},\"handle_type\":0,\"interpolation\":0}]},\"green\":{\"Points\":[{\"co\":{\"X\":1,\"Y\":123},\"handle_left\":{\"X\":0.5,\"Y\":1},\"handle_right\":{\"X\":0.5,\"Y\":0},\"handle_type\":0,\"interpolation\":0}]},\"red\":{\"Points\":[{\"co\":{\"X\":1,\"Y\":0},\"handle_left\":{\"X\":0.5,\"Y\":1},\"handle_right\":{\"X\":0.5,\"Y\":0},\"handle_type\":0,\"interpolation\":0}]}},\"waveform\":false,\"file_id\":\"XL7T80Y9R1\",\"title\":\"sintel_trailer-720p.mp4\",\"id\":\"KQK39ZFGJE\",\"image\":\"@assets/thumbnail/XL7T80Y9R1.png\"},{\"alpha\":{\"Points\":[{\"co\":{\"X\":1,\"Y\":1},\"handle_left\":{\"X\":0.5,\"Y\":1},\"handle_right\":{\"X\":0.5,\"Y\":0},\"handle_type\":0,\"interpolation\":0}]},\"anchor\":0,\"channel_filter\":{\"Points\":[{\"co\":{\"X\":1,\"Y\":-1},\"handle_left\":{\"X\":0.5,\"Y\":1},\"handle_right\":{\"X\":0.5,\"Y\":0},\"handle_type\":0,\"interpolation\":0}]},\"channel_mapping\":{\"Points\":[{\"co\":{\"X\":1,\"Y\":-1},\"handle_left\":{\"X\":0.5,\"Y\":1},\"handle_right\":{\"X\":0.5,\"Y\":0},\"handle_type\":0,\"interpolation\":0}]},\"display\":0,\"duration\":51.9466667175293,\"effects\":[],\"end\":29.566666666666666,\"gravity\":4,\"has_audio\":{\"Points\":[{\"co\":{\"X\":1,\"Y\":-1},\"handle_left\":{\"X\":0.5,\"Y\":1},\"handle_right\":{\"X\":0.5,\"Y\":0},\"handle_type\":0,\"interpolation\":0}]},\"has_video\":{\"Points\":[{\"co\":{\"X\":1,\"Y\":-1},\"handle_left\":{\"X\":0.5,\"Y\":1},\"handle_right\":{\"X\":0.5,\"Y\":0},\"handle_type\":0,\"interpolation\":0}]},\"layer\":5000000,\"location_x\":{\"Points\":[{\"co\":{\"X\":1,\"Y\":0},\"handle_left\":{\"X\":0.5,\"Y\":1},\"handle_right\":{\"X\":0.5,\"Y\":0},\"handle_type\":0,\"interpolation\":0}]},\"location_y\":{\"Points\":[{\"co\":{\"X\":1,\"Y\":0},\"handle_left\":{\"X\":0.5,\"Y\":1},\"handle_right\":{\"X\":0.5,\"Y\":0},\"handle_type\":0,\"interpolation\":0}]},\"mixing\":0,\"origin_x\":{\"Points\":[{\"co\":{\"X\":1,\"Y\":0.5},\"handle_left\":{\"X\":0.5,\"Y\":1},\"handle_right\":{\"X\":0.5,\"Y\":0},\"handle_type\":0,\"interpolation\":0}]},\"origin_y\":{\"Points\":[{\"co\":{\"X\":1,\"Y\":0.5},\"handle_left\":{\"X\":0.5,\"Y\":1},\"handle_right\":{\"X\":0.5,\"Y\":0},\"handle_type\":0,\"interpolation\":0}]},\"parentObjectId\":\"\",\"perspective_c1_x\":{\"Points\":[{\"co\":{\"X\":1,\"Y\":-1},\"handle_left\":{\"X\":0.5,\"Y\":1},\"handle_right\":{\"X\":0.5,\"Y\":0},\"handle_type\":0,\"interpolation\":0}]},\"perspective_c1_y\":{\"Points\":[{\"co\":{\"X\":1,\"Y\":-1},\"handle_left\":{\"X\":0.5,\"Y\":1},\"handle_right\":{\"X\":0.5,\"Y\":0},\"handle_type\":0,\"interpolation\":0}]},\"perspective_c2_x\":{\"Points\":[{\"co\":{\"X\":1,\"Y\":-1},\"handle_left\":{\"X\":0.5,\"Y\":1},\"handle_right\":{\"X\":0.5,\"Y\":0},\"handle_type\":0,\"interpolation\":0}]},\"perspective_c2_y\":{\"Points\":[{\"co\":{\"X\":1,\"Y\":-1},\"handle_left\":{\"X\":0.5,\"Y\":1},\"handle_right\":{\"X\":0.5,\"Y\":0},\"handle_type\":0,\"interpolation\":0}]},\"perspective_c3_x\":{\"Points\":[{\"co\":{\"X\":1,\"Y\":-1},\"handle_left\":{\"X\":0.5,\"Y\":1},\"handle_right\":{\"X\":0.5,\"Y\":0},\"handle_type\":0,\"interpolation\":0}]},\"perspective_c3_y\":{\"Points\":[{\"co\":{\"X\":1,\"Y\":-1},\"handle_left\":{\"X\":0.5,\"Y\":1},\"handle_right\":{\"X\":0.5,\"Y\":0},\"handle_type\":0,\"interpolation\":0}]},\"perspective_c4_x\":{\"Points\":[{\"co\":{\"X\":1,\"Y\":-1},\"handle_left\":{\"X\":0.5,\"Y\":1},\"handle_right\":{\"X\":0.5,\"Y\":0},\"handle_type\":0,\"interpolation\":0}]},\"perspective_c4_y\":{\"Points\":[{\"co\":{\"X\":1,\"Y\":-1},\"handle_left\":{\"X\":0.5,\"Y\":1},\"handle_right\":{\"X\":0.5,\"Y\":0},\"handle_type\":0,\"interpolation\":0}]},\"position\":12.3,\"reader\":{\"acodec\":\"aac\",\"audio_bit_rate\":126694,\"audio_stream_index\":1,\"audio_timebase\":{\"den\":48000,\"num\":1},\"channel_layout\":3,\"channels\":2,\"display_ratio\":{\"den\":9,\"num\":16},\"duration\":51.9466667175293,\"file_size\":\"7608204\",\"fps\":{\"den\":1,\"num\":24},\"has_audio\":true,\"has_single_image\":false,\"has_video\":true,\"height\":720,\"interlaced_frame\":false,\"metadata\":{\"artist\":\"Durian Open Movie Team\",\"compatible_brands\":\"isomiso2avc1mp41\",\"copyright\":\"(c) copyright Blender Foundation | durian.blender.org\",\"creation_time\":\"1970-01-01T00:00:00.000000Z\",\"description\":\"Trailer for the Sintel open movie project\",\"encoder\":\"Lavf52.62.0\",\"handler_name\":\"SoundHandler\",\"language\":\"und\",\"major_brand\":\"isom\",\"minor_version\":\"512\",\"title\":\"Sintel Trailer\"},\"path\":\"" << TEST_MEDIA_PATH << "sintel_trailer-720p.mp4\",\"pixel_format\":0,\"pixel_ratio\":{\"den\":1,\"num\":1},\"sample_rate\":48000,\"top_field_first\":true,\"type\":\"FFmpegReader\",\"vcodec\":\"h264\",\"video_bit_rate\":145725,\"video_length\":\"1253\",\"video_stream_index\":0,\"video_timebase\":{\"den\":24,\"num\":1},\"width\":1280},\"rotation\":{\"Points\":[{\"co\":{\"X\":1,\"Y\":0},\"handle_left\":{\"X\":0.5,\"Y\":1},\"handle_right\":{\"X\":0.5,\"Y\":0},\"handle_type\":0,\"interpolation\":0}]},\"scale\":1,\"scale_x\":{\"Points\":[{\"co\":{\"X\":1,\"Y\":1},\"handle_left\":{\"X\":0.5,\"Y\":1},\"handle_right\":{\"X\":0.5,\"Y\":0},\"handle_type\":0,\"interpolation\":0}]},\"scale_y\":{\"Points\":[{\"co\":{\"X\":1,\"Y\":1},\"handle_left\":{\"X\":0.5,\"Y\":1},\"handle_right\":{\"X\":0.5,\"Y\":0},\"handle_type\":0,\"interpolation\":0}]},\"shear_x\":{\"Points\":[{\"co\":{\"X\":1,\"Y\":0},\"handle_left\":{\"X\":0.5,\"Y\":1},\"handle_right\":{\"X\":0.5,\"Y\":0},\"handle_type\":0,\"interpolation\":0}]},\"shear_y\":{\"Points\":[{\"co\":{\"X\":1,\"Y\":0},\"handle_left\":{\"X\":0.5,\"Y\":1},\"handle_right\":{\"X\":0.5,\"Y\":0},\"handle_type\":0,\"interpolation\":0}]},\"start\":20.866666666666667,\"time\":{\"Points\":[{\"co\":{\"X\":1,\"Y\":1},\"handle_left\":{\"X\":0.5,\"Y\":1},\"handle_right\":{\"X\":0.5,\"Y\":0},\"handle_type\":0,\"interpolation\":0}]},\"volume\":{\"Points\":[{\"co\":{\"X\":1,\"Y\":1},\"handle_left\":{\"X\":0.5,\"Y\":1},\"handle_right\":{\"X\":0.5,\"Y\":0},\"handle_type\":0,\"interpolation\":0}]},\"wave_color\":{\"alpha\":{\"Points\":[{\"co\":{\"X\":1,\"Y\":255},\"handle_left\":{\"X\":0.5,\"Y\":1},\"handle_right\":{\"X\":0.5,\"Y\":0},\"handle_type\":0,\"interpolation\":0}]},\"blue\":{\"Points\":[{\"co\":{\"X\":1,\"Y\":255},\"handle_left\":{\"X\":0.5,\"Y\":1},\"handle_right\":{\"X\":0.5,\"Y\":0},\"handle_type\":0,\"interpolation\":0}]},\"green\":{\"Points\":[{\"co\":{\"X\":1,\"Y\":123},\"handle_left\":{\"X\":0.5,\"Y\":1},\"handle_right\":{\"X\":0.5,\"Y\":0},\"handle_type\":0,\"interpolation\":0}]},\"red\":{\"Points\":[{\"co\":{\"X\":1,\"Y\":0},\"handle_left\":{\"X\":0.5,\"Y\":1},\"handle_right\":{\"X\":0.5,\"Y\":0},\"handle_type\":0,\"interpolation\":0}]}},\"waveform\":false,\"file_id\":\"XL7T80Y9R1\",\"title\":\"sintel_trailer-720p.mp4\",\"id\":\"TMKI8CK7QQ\",\"image\":\"@assets/thumbnail/XL7T80Y9R1.png\"},{\"alpha\":{\"Points\":[{\"co\":{\"X\":1,\"Y\":0},\"handle_left\":{\"X\":0.5,\"Y\":1},\"handle_right\":{\"X\":0.5,\"Y\":0},\"handle_type\":0,\"interpolation\":0},{\"co\":{\"X\":91,\"Y\":1},\"handle_left\":{\"X\":0.5,\"Y\":1},\"handle_right\":{\"X\":0.5,\"Y\":0},\"handle_type\":0,\"interpolation\":0},{\"co\":{\"X\":541,\"Y\":1},\"handle_left\":{\"X\":0.5,\"Y\":1},\"handle_right\":{\"X\":0.5,\"Y\":0},\"handle_type\":0,\"interpolation\":0},{\"co\":{\"X\":631,\"Y\":0},\"handle_left\":{\"X\":0.5,\"Y\":1},\"handle_right\":{\"X\":0.5,\"Y\":0},\"handle_type\":0,\"interpolation\":0}]},\"anchor\":0,\"channel_filter\":{\"Points\":[{\"co\":{\"X\":1,\"Y\":-1},\"handle_left\":{\"X\":0.5,\"Y\":1},\"handle_right\":{\"X\":0.5,\"Y\":0},\"handle_type\":0,\"interpolation\":0}]},\"channel_mapping\":{\"Points\":[{\"co\":{\"X\":1,\"Y\":-1},\"handle_left\":{\"X\":0.5,\"Y\":1},\"handle_right\":{\"X\":0.5,\"Y\":0},\"handle_type\":0,\"interpolation\":0}]},\"display\":0,\"duration\":3600,\"effects\":[],\"end\":21,\"gravity\":4,\"has_audio\":{\"Points\":[{\"co\":{\"X\":1,\"Y\":-1},\"handle_left\":{\"X\":0.5,\"Y\":1},\"handle_right\":{\"X\":0.5,\"Y\":0},\"handle_type\":0,\"interpolation\":0}]},\"has_video\":{\"Points\":[{\"co\":{\"X\":1,\"Y\":-1},\"handle_left\":{\"X\":0.5,\"Y\":1},\"handle_right\":{\"X\":0.5,\"Y\":0},\"handle_type\":0,\"interpolation\":0}]},\"id\":\"2CQVCHPATF\",\"layer\":6000000,\"location_x\":{\"Points\":[{\"co\":{\"X\":1,\"Y\":0},\"handle_left\":{\"X\":0.5,\"Y\":1},\"handle_right\":{\"X\":0.5,\"Y\":0},\"handle_type\":0,\"interpolation\":0}]},\"location_y\":{\"Points\":[{\"co\":{\"X\":1,\"Y\":0},\"handle_left\":{\"X\":0.5,\"Y\":1},\"handle_right\":{\"X\":0.5,\"Y\":0},\"handle_type\":0,\"interpolation\":0}]},\"mixing\":0,\"origin_x\":{\"Points\":[{\"co\":{\"X\":1,\"Y\":0.5},\"handle_left\":{\"X\":0.5,\"Y\":1},\"handle_right\":{\"X\":0.5,\"Y\":0},\"handle_type\":0,\"interpolation\":0}]},\"origin_y\":{\"Points\":[{\"co\":{\"X\":1,\"Y\":0.5},\"handle_left\":{\"X\":0.5,\"Y\":1},\"handle_right\":{\"X\":0.5,\"Y\":0},\"handle_type\":0,\"interpolation\":0}]},\"parentObjectId\":\"\",\"perspective_c1_x\":{\"Points\":[{\"co\":{\"X\":1,\"Y\":-1},\"handle_left\":{\"X\":0.5,\"Y\":1},\"handle_right\":{\"X\":0.5,\"Y\":0},\"handle_type\":0,\"interpolation\":0}]},\"perspective_c1_y\":{\"Points\":[{\"co\":{\"X\":1,\"Y\":-1},\"handle_left\":{\"X\":0.5,\"Y\":1},\"handle_right\":{\"X\":0.5,\"Y\":0},\"handle_type\":0,\"interpolation\":0}]},\"perspective_c2_x\":{\"Points\":[{\"co\":{\"X\":1,\"Y\":-1},\"handle_left\":{\"X\":0.5,\"Y\":1},\"handle_right\":{\"X\":0.5,\"Y\":0},\"handle_type\":0,\"interpolation\":0}]},\"perspective_c2_y\":{\"Points\":[{\"co\":{\"X\":1,\"Y\":-1},\"handle_left\":{\"X\":0.5,\"Y\":1},\"handle_right\":{\"X\":0.5,\"Y\":0},\"handle_type\":0,\"interpolation\":0}]},\"perspective_c3_x\":{\"Points\":[{\"co\":{\"X\":1,\"Y\":-1},\"handle_left\":{\"X\":0.5,\"Y\":1},\"handle_right\":{\"X\":0.5,\"Y\":0},\"handle_type\":0,\"interpolation\":0}]},\"perspective_c3_y\":{\"Points\":[{\"co\":{\"X\":1,\"Y\":-1},\"handle_left\":{\"X\":0.5,\"Y\":1},\"handle_right\":{\"X\":0.5,\"Y\":0},\"handle_type\":0,\"interpolation\":0}]},\"perspective_c4_x\":{\"Points\":[{\"co\":{\"X\":1,\"Y\":-1},\"handle_left\":{\"X\":0.5,\"Y\":1},\"handle_right\":{\"X\":0.5,\"Y\":0},\"handle_type\":0,\"interpolation\":0}]},\"perspective_c4_y\":{\"Points\":[{\"co\":{\"X\":1,\"Y\":-1},\"handle_left\":{\"X\":0.5,\"Y\":1},\"handle_right\":{\"X\":0.5,\"Y\":0},\"handle_type\":0,\"interpolation\":0}]},\"position\":0,\"reader\":{\"acodec\":\"\",\"audio_bit_rate\":0,\"audio_stream_index\":-1,\"audio_timebase\":{\"den\":1,\"num\":1},\"channel_layout\":4,\"channels\":0,\"display_ratio\":{\"den\":2,\"num\":3},\"duration\":3600,\"file_size\":\"1382400\",\"fps\":{\"den\":1,\"num\":30},\"has_audio\":false,\"has_single_image\":true,\"has_video\":true,\"height\":480,\"interlaced_frame\":false,\"metadata\":{},\"path\":\"" << TEST_MEDIA_PATH << "front3.png\",\"pixel_format\":-1,\"pixel_ratio\":{\"den\":1,\"num\":1},\"sample_rate\":0,\"top_field_first\":true,\"type\":\"QtImageReader\",\"vcodec\":\"\",\"video_bit_rate\":0,\"video_length\":\"108000\",\"video_stream_index\":-1,\"video_timebase\":{\"den\":30,\"num\":1},\"width\":720},\"rotation\":{\"Points\":[{\"co\":{\"X\":1,\"Y\":0},\"handle_left\":{\"X\":0.5,\"Y\":1},\"handle_right\":{\"X\":0.5,\"Y\":0},\"handle_type\":0,\"interpolation\":0}]},\"scale\":1,\"scale_x\":{\"Points\":[{\"co\":{\"X\":1,\"Y\":1},\"handle_left\":{\"X\":0.5,\"Y\":1},\"handle_right\":{\"X\":0.5,\"Y\":0},\"handle_type\":0,\"interpolation\":0}]},\"scale_y\":{\"Points\":[{\"co\":{\"X\":1,\"Y\":1},\"handle_left\":{\"X\":0.5,\"Y\":1},\"handle_right\":{\"X\":0.5,\"Y\":0},\"handle_type\":0,\"interpolation\":0}]},\"shear_x\":{\"Points\":[{\"co\":{\"X\":1,\"Y\":0},\"handle_left\":{\"X\":0.5,\"Y\":1},\"handle_right\":{\"X\":0.5,\"Y\":0},\"handle_type\":0,\"interpolation\":0}]},\"shear_y\":{\"Points\":[{\"co\":{\"X\":1,\"Y\":0},\"handle_left\":{\"X\":0.5,\"Y\":1},\"handle_right\":{\"X\":0.5,\"Y\":0},\"handle_type\":0,\"interpolation\":0}]},\"start\":0,\"time\":{\"Points\":[{\"co\":{\"X\":1,\"Y\":1},\"handle_left\":{\"X\":0.5,\"Y\":1},\"handle_right\":{\"X\":0.5,\"Y\":0},\"handle_type\":0,\"interpolation\":0}]},\"volume\":{\"Points\":[{\"co\":{\"X\":1,\"Y\":1},\"handle_left\":{\"X\":0.5,\"Y\":1},\"handle_right\":{\"X\":0.5,\"Y\":0},\"handle_type\":0,\"interpolation\":0}]},\"wave_color\":{\"alpha\":{\"Points\":[{\"co\":{\"X\":1,\"Y\":255},\"handle_left\":{\"X\":0.5,\"Y\":1},\"handle_right\":{\"X\":0.5,\"Y\":0},\"handle_type\":0,\"interpolation\":0}]},\"blue\":{\"Points\":[{\"co\":{\"X\":1,\"Y\":255},\"handle_left\":{\"X\":0.5,\"Y\":1},\"handle_right\":{\"X\":0.5,\"Y\":0},\"handle_type\":0,\"interpolation\":0}]},\"green\":{\"Points\":[{\"co\":{\"X\":1,\"Y\":123},\"handle_left\":{\"X\":0.5,\"Y\":1},\"handle_right\":{\"X\":0.5,\"Y\":0},\"handle_type\":0,\"interpolation\":0}]},\"red\":{\"Points\":[{\"co\":{\"X\":1,\"Y\":0},\"handle_left\":{\"X\":0.5,\"Y\":1},\"handle_right\":{\"X\":0.5,\"Y\":0},\"handle_type\":0,\"interpolation\":0}]}},\"waveform\":false,\"file_id\":\"RY3OYWU7HK\",\"title\":\"front3.png\",\"image\":\"@assets/thumbnail/RY3OYWU7HK.png\"}],\"effects\":[{\"id\":\"335XHEZJNX\",\"layer\":5000000,\"title\":\"Transition\",\"type\":\"Mask\",\"position\":5.7,\"start\":0,\"end\":4.966666666666666,\"brightness\":{\"Points\":[{\"co\":{\"X\":1,\"Y\":1},\"handle_left\":{\"X\":0.5,\"Y\":1},\"handle_right\":{\"X\":0.5,\"Y\":0},\"handle_type\":0,\"interpolation\":0},{\"co\":{\"X\":150,\"Y\":-1},\"handle_left\":{\"X\":0.5,\"Y\":1},\"handle_right\":{\"X\":0.5,\"Y\":0},\"handle_type\":0,\"interpolation\":0}]},\"contrast\":{\"Points\":[{\"co\":{\"X\":1,\"Y\":3},\"handle_left\":{\"X\":0.5,\"Y\":1},\"handle_right\":{\"X\":0.5,\"Y\":0},\"handle_type\":0,\"interpolation\":0}]},\"reader\":{\"acodec\":\"\",\"audio_bit_rate\":0,\"audio_stream_index\":-1,\"audio_timebase\":{\"den\":1,\"num\":1},\"channel_layout\":4,\"channels\":0,\"display_ratio\":{\"den\":4,\"num\":5},\"duration\":3600,\"file_size\":\"5832000\",\"fps\":{\"den\":1,\"num\":30},\"has_audio\":false,\"has_single_image\":true,\"has_video\":true,\"height\":576,\"interlaced_frame\":false,\"metadata\":{},\"path\":\"" << TEST_MEDIA_PATH << "mask.png\",\"pixel_format\":-1,\"pixel_ratio\":{\"den\":1,\"num\":1},\"sample_rate\":0,\"top_field_first\":true,\"type\":\"QtImageReader\",\"vcodec\":\"\",\"video_bit_rate\":0,\"video_length\":\"108000\",\"video_stream_index\":-1,\"video_timebase\":{\"den\":30,\"num\":1},\"width\":720},\"replace_image\":false},{\"id\":\"QQECKBIYUP\",\"layer\":5000000,\"title\":\"Transition\",\"type\":\"Mask\",\"position\":12.3,\"start\":0,\"end\":3.6000000000000014,\"brightness\":{\"Points\":[{\"co\":{\"X\":1,\"Y\":1},\"handle_left\":{\"X\":0.5,\"Y\":1},\"handle_right\":{\"X\":0.5,\"Y\":0},\"handle_type\":0,\"interpolation\":0},{\"co\":{\"X\":109,\"Y\":-1},\"handle_left\":{\"X\":0.5,\"Y\":1},\"handle_right\":{\"X\":0.5,\"Y\":0},\"handle_type\":0,\"interpolation\":0}]},\"contrast\":{\"Points\":[{\"co\":{\"X\":1,\"Y\":3},\"handle_left\":{\"X\":0.5,\"Y\":1},\"handle_right\":{\"X\":0.5,\"Y\":0},\"handle_type\":0,\"interpolation\":0}]},\"reader\":{\"acodec\":\"\",\"audio_bit_rate\":0,\"audio_stream_index\":-1,\"audio_timebase\":{\"den\":1,\"num\":1},\"channel_layout\":4,\"channels\":0,\"display_ratio\":{\"den\":4,\"num\":5},\"duration\":3600,\"file_size\":\"5832000\",\"fps\":{\"den\":1,\"num\":30},\"has_audio\":false,\"has_single_image\":true,\"has_video\":true,\"height\":576,\"interlaced_frame\":false,\"metadata\":{},\"path\":\"" << TEST_MEDIA_PATH << "mask.png\",\"pixel_format\":-1,\"pixel_ratio\":{\"den\":1,\"num\":1},\"sample_rate\":0,\"top_field_first\":true,\"type\":\"QtImageReader\",\"vcodec\":\"\",\"video_bit_rate\":0,\"video_length\":\"108000\",\"video_stream_index\":-1,\"video_timebase\":{\"den\":30,\"num\":1},\"width\":720},\"replace_image\":false},{\"id\":\"YELU1J5KI8\",\"layer\":5000000,\"title\":\"Transition\",\"type\":\"Mask\",\"position\":17.7,\"start\":0,\"end\":3.3000000000000007,\"brightness\":{\"Points\":[{\"co\":{\"X\":1,\"Y\":1},\"handle_left\":{\"X\":0.5,\"Y\":1},\"handle_right\":{\"X\":0.5,\"Y\":0},\"handle_type\":0,\"interpolation\":0},{\"co\":{\"X\":100,\"Y\":-1},\"handle_left\":{\"X\":0.5,\"Y\":1},\"handle_right\":{\"X\":0.5,\"Y\":0},\"handle_type\":0,\"interpolation\":0}]},\"contrast\":{\"Points\":[{\"co\":{\"X\":1,\"Y\":3},\"handle_left\":{\"X\":0.5,\"Y\":1},\"handle_right\":{\"X\":0.5,\"Y\":0},\"handle_type\":0,\"interpolation\":0}]},\"reader\":{\"acodec\":\"\",\"audio_bit_rate\":0,\"audio_stream_index\":-1,\"audio_timebase\":{\"den\":1,\"num\":1},\"channel_layout\":4,\"channels\":0,\"display_ratio\":{\"den\":4,\"num\":5},\"duration\":3600,\"file_size\":\"5832000\",\"fps\":{\"den\":1,\"num\":30},\"has_audio\":false,\"has_single_image\":true,\"has_video\":true,\"height\":576,\"interlaced_frame\":false,\"metadata\":{},\"path\":\"" << TEST_MEDIA_PATH << "mask.png\",\"pixel_format\":-1,\"pixel_ratio\":{\"den\":1,\"num\":1},\"sample_rate\":0,\"top_field_first\":true,\"type\":\"QtImageReader\",\"vcodec\":\"\",\"video_bit_rate\":0,\"video_length\":\"108000\",\"video_stream_index\":-1,\"video_timebase\":{\"den\":30,\"num\":1},\"width\":720},\"replace_image\":false}],\"duration\":300,\"scale\":15,\"tick_pixels\":100,\"playhead_position\":0,\"profile\":\"HD 720p 30 fps\",\"layers\":[{\"id\":\"L1\",\"label\":\"\",\"number\":1000000,\"y\":0,\"lock\":false},{\"id\":\"L2\",\"label\":\"\",\"number\":2000000,\"y\":0,\"lock\":false},{\"id\":\"L3\",\"label\":\"\",\"number\":3000000,\"y\":0,\"lock\":false},{\"id\":\"L4\",\"label\":\"\",\"number\":4000000,\"y\":0,\"lock\":false},{\"id\":\"L5\",\"label\":\"\",\"number\":5000000,\"y\":0,\"lock\":false},{\"number\":6000000,\"y\":0,\"label\":\"\",\"lock\":false,\"id\":\"4U4NB9QVD2\"}],\"markers\":[],\"progress\":[],\"version\":{\"openshot-qt\":\"2.6.1-dev\",\"libopenshot\":\"0.2.7-dev\"}}";
	t->SetJson(project_json.str());
	t->Open();

	// A successful test will NOT crash - since this causes many threads to
	// call the same Timeline methods asynchronously, to verify mutexes and multi-threaded
	// access does not seg fault or crash this test.
#pragma omp parallel
	{
		// Run the following loop in all threads
		int64_t frame_count = 60;
		for (long int frame = 1; frame <= frame_count; frame++) {
			 std::shared_ptr<Frame> f = t->GetFrame(frame);

			// Clear cache after every frame
			// This is designed to test the mutex for ClearAllCache()
			 t->ClearAllCache();
		}
		 // Clear all clips after loop is done
		// This is designed to test the mutex for Clear()
		t->Clear();
	}

	// Close and delete timeline object
	t->Close();
	delete t;
	t = NULL;
}

// ---------------------------------------------------------------------------
// New tests to validate removing timeline-level effects (incl. threading/locks)
// Paste at the end of tests/Timeline.cpp
// ---------------------------------------------------------------------------

TEST_CASE( "RemoveEffect basic", "[libopenshot][timeline]" )
{
	// Create a simple timeline
	Timeline t(640, 480, Fraction(30, 1), 44100, 2, LAYOUT_STEREO);

	// Two timeline-level effects
	Negate e1; e1.Id("E1"); e1.Layer(0);
	Negate e2; e2.Id("E2"); e2.Layer(1);

	t.AddEffect(&e1);
	t.AddEffect(&e2);

	// Sanity check
	REQUIRE(t.Effects().size() == 2);
	REQUIRE(t.GetEffect("E1") != nullptr);
	REQUIRE(t.GetEffect("E2") != nullptr);

	// Remove one effect and verify it is truly gone
	t.RemoveEffect(&e1);
	auto effects_after = t.Effects();
	CHECK(effects_after.size() == 1);
	CHECK(t.GetEffect("E1") == nullptr);
	CHECK(t.GetEffect("E2") != nullptr);
	CHECK(std::find(effects_after.begin(), effects_after.end(), &e1) == effects_after.end());

	// Removing the same (already-removed) effect should be a no-op
	t.RemoveEffect(&e1);
	CHECK(t.Effects().size() == 1);
}

TEST_CASE( "RemoveEffect not present is no-op", "[libopenshot][timeline]" )
{
	Timeline t(640, 480, Fraction(30, 1), 44100, 2, LAYOUT_STEREO);

	Negate existing; existing.Id("KEEP"); existing.Layer(0);
	Negate never_added; never_added.Id("GHOST"); never_added.Layer(1);

	t.AddEffect(&existing);
	REQUIRE(t.Effects().size() == 1);

	// Try to remove an effect pointer that was never added
	t.RemoveEffect(&never_added);

	// State should be unchanged
	CHECK(t.Effects().size() == 1);
	CHECK(t.GetEffect("KEEP") != nullptr);
	CHECK(t.GetEffect("GHOST") == nullptr);
}

TEST_CASE( "RemoveEffect while open (active pipeline safety)", "[libopenshot][timeline]" )
{
	// Timeline with one visible clip so we can request frames
	Timeline t(640, 480, Fraction(30, 1), 44100, 2, LAYOUT_STEREO);
	std::stringstream path;
	path << TEST_MEDIA_PATH << "front3.png";
	Clip clip(path.str());
	clip.Layer(0);
	t.AddClip(&clip);

	// Add a timeline-level effect and open the timeline
	Negate neg; neg.Id("NEG"); neg.Layer(1);
	t.AddEffect(&neg);

	t.Open();
	// Touch the pipeline before removal
	std::shared_ptr<Frame> f1 = t.GetFrame(1);
	REQUIRE(f1 != nullptr);

	// Remove the effect while open, this should be safe and effective
	t.RemoveEffect(&neg);
	CHECK(t.GetEffect("NEG") == nullptr);
	CHECK(t.Effects().size() == 0);

	// Touch the pipeline again after removal (should not crash / deadlock)
	std::shared_ptr<Frame> f2 = t.GetFrame(2);
	REQUIRE(f2 != nullptr);

		// Close reader
	t.Close();
}

TEST_CASE( "RemoveEffect preserves ordering of remaining effects", "[libopenshot][timeline]" )
{
	// Create a timeline
	Timeline t(640, 480, Fraction(30, 1), 44100, 2, LAYOUT_STEREO);

	// Add effects out of order (Layer/Position/Order)
	Negate a; a.Id("A"); a.Layer(0); a.Position(0.0); a.Order(0);
	Negate b1; b1.Id("B-1"); b1.Layer(1); b1.Position(0.0); b1.Order(3);
	Negate b;  b.Id("B");  b.Layer(1); b.Position(0.0); b.Order(0);
	Negate b2; b2.Id("B-2"); b2.Layer(1); b2.Position(0.5); b2.Order(2);
	Negate b3; b3.Id("B-3"); b3.Layer(1); b3.Position(0.5); b3.Order(1);
	Negate c;  c.Id("C");  c.Layer(2); c.Position(0.0); c.Order(0);

	t.AddEffect(&c);
	t.AddEffect(&b);
	t.AddEffect(&a);
	t.AddEffect(&b3);
	t.AddEffect(&b2);
	t.AddEffect(&b1);

	// Remove a middle effect and verify ordering is still deterministic
	t.RemoveEffect(&b);

	std::list<EffectBase*> effects = t.Effects();
	REQUIRE(effects.size() == 5);

	int n = 0;
	for (auto effect : effects) {
		switch (n) {
		case 0:
			CHECK(effect->Layer() == 0);
			CHECK(effect->Id() == "A");
			CHECK(effect->Order() == 0);
			break;
		case 1:
			CHECK(effect->Layer() == 1);
			CHECK(effect->Id() == "B-1");
			CHECK(effect->Position() == Approx(0.0).margin(0.0001));
			CHECK(effect->Order() == 3);
			break;
		case 2:
			CHECK(effect->Layer() == 1);
			CHECK(effect->Id() == "B-2");
			CHECK(effect->Position() == Approx(0.5).margin(0.0001));
			CHECK(effect->Order() == 2);
			break;
		case 3:
			CHECK(effect->Layer() == 1);
			CHECK(effect->Id() == "B-3");
			CHECK(effect->Position() == Approx(0.5).margin(0.0001));
			CHECK(effect->Order() == 1);
			break;
		case 4:
			CHECK(effect->Layer() == 2);
			CHECK(effect->Id() == "C");
			CHECK(effect->Order() == 0);
			break;
		}
		++n;
	}
}

TEST_CASE( "Multi-threaded Timeline Add/Remove Effect", "[libopenshot][timeline]" )
{
	// Create timeline with a clip so frames can be requested
	Timeline *t = new Timeline(1280, 720, Fraction(24, 1), 48000, 2, LAYOUT_STEREO);
	std::stringstream path;
	path << TEST_MEDIA_PATH << "test.mp4";
	Clip *clip = new Clip(path.str());
	clip->Layer(0);
	t->AddClip(clip);
	t->Open();

	// A successful test will NOT crash - many threads will add/remove effects
	// while also requesting frames, exercising locks around effect mutation.
#pragma omp parallel
	{
		int64_t effect_count = 10;
		for (int i = 0; i < effect_count; ++i) {
			// Each thread creates its own effect
			Negate *neg = new Negate();
			std::stringstream sid;
			sid << "NEG_T" << omp_get_thread_num() << "_I" << i;
			neg->Id(sid.str());
			neg->Layer(1 + omp_get_thread_num()); // spread across layers

			// Add the effect
			t->AddEffect(neg);

			// Touch a few frames to exercise the render pipeline with the effect
			for (long int frame = 1; frame <= 6; ++frame) {
				std::shared_ptr<Frame> f = t->GetFrame(frame);
				REQUIRE(f != nullptr);
			}

			// Remove the effect and destroy it
			t->RemoveEffect(neg);
			delete neg;
			neg = nullptr;
		}

		// Clear all effects at the end from within threads (should be safe)
		// This also exercises internal sorting/locking paths
		t->Clear();
	}

	t->Close();
	delete t;
	t = nullptr;
	delete clip;
	clip = nullptr;
}

TEST_CASE( "ApplyJSONDiff and FrameMappers", "[libopenshot][timeline]" )
{
	// Create a timeline
	Timeline t(640, 480, Fraction(60, 1), 44100, 2, LAYOUT_STEREO);
	t.Open();

	// Auto create FrameMappers for each clip
	t.AutoMapClips(true);

	// Add clip
	std::stringstream path1;
	path1 << TEST_MEDIA_PATH << "interlaced.png";
	Clip clip1(path1.str());
	clip1.Id("ABC");
	clip1.Layer(1);
	clip1.Position(0);
	clip1.End(10);

	// Verify clip reader type (not wrapped yet, because we have not added clip to timeline)
	CHECK(clip1.Reader()->Name() == "QtImageReader");

	t.AddClip(&clip1);

	// Verify clip was wrapped in FrameMapper
	CHECK(clip1.Reader()->Name() == "FrameMapper");

	// Update Clip's basic properties with JSON Diff (i.e. no reader JSON)
	std::stringstream json_change1;
	json_change1 << "[{\"type\":\"update\",\"key\":[\"clips\",{\"id\":\"" << clip1.Id() << "\"}],\"value\":{\"id\":\"" << clip1.Id() << "\",\"layer\":4000000,\"position\":14.7,\"start\":0,\"end\":10},\"partial\":false}]";
	t.ApplyJsonDiff(json_change1.str());

	// Verify clip is still wrapped in FrameMapper
	CHECK(clip1.Reader()->Name() == "FrameMapper");

	// Update clip's reader back to a QtImageReader
	std::stringstream json_change2;
	json_change2 << "[{\"type\":\"update\",\"key\":[\"clips\",{\"id\":\"" << clip1.Id() << "\"}],\"value\":{\"id\":\"" << clip1.Id() << "\",\"reader\":{\"acodec\":\"\",\"audio_bit_rate\":0,\"audio_stream_index\":-1,\"audio_timebase\":{\"den\":1,\"num\":1},\"channel_layout\":4,\"channels\":0,\"display_ratio\":{\"den\":1,\"num\":1},\"duration\":3600.0,\"file_size\":\"160000\",\"fps\":{\"den\":1,\"num\":30},\"has_audio\":false,\"has_single_image\":true,\"has_video\":true,\"height\":200,\"interlaced_frame\":false,\"metadata\":{},\"path\":\"" << path1.str() << "\",\"pixel_format\":-1,\"pixel_ratio\":{\"den\":1,\"num\":1},\"sample_rate\":0,\"top_field_first\":true,\"type\":\"QtImageReader\",\"vcodec\":\"\",\"video_bit_rate\":0,\"video_length\":\"108000\",\"video_stream_index\":-1,\"video_timebase\":{\"den\":30,\"num\":1},\"width\":200},\"position\":14.7,\"start\":0,\"end\":10},\"partial\":false}]";
	t.ApplyJsonDiff(json_change2.str());

	// Verify clip reader type
	CHECK(clip1.Reader()->Name() == "FrameMapper");

	// Disable Auto FrameMappers for each clip
	t.AutoMapClips(false);

	// Update clip's reader back to a QtImageReader
	std::stringstream json_change3;
	json_change3 << "[{\"type\":\"update\",\"key\":[\"clips\",{\"id\":\"" << clip1.Id() << "\"}],\"value\":{\"id\":\"" << clip1.Id() << "\",\"reader\":{\"acodec\":\"\",\"audio_bit_rate\":0,\"audio_stream_index\":-1,\"audio_timebase\":{\"den\":1,\"num\":1},\"channel_layout\":4,\"channels\":0,\"display_ratio\":{\"den\":1,\"num\":1},\"duration\":3600.0,\"file_size\":\"160000\",\"fps\":{\"den\":1,\"num\":30},\"has_audio\":false,\"has_single_image\":true,\"has_video\":true,\"height\":200,\"interlaced_frame\":false,\"metadata\":{},\"path\":\"" << path1.str() << "\",\"pixel_format\":-1,\"pixel_ratio\":{\"den\":1,\"num\":1},\"sample_rate\":0,\"top_field_first\":true,\"type\":\"QtImageReader\",\"vcodec\":\"\",\"video_bit_rate\":0,\"video_length\":\"108000\",\"video_stream_index\":-1,\"video_timebase\":{\"den\":30,\"num\":1},\"width\":200},\"position\":14.7,\"start\":0,\"end\":10},\"partial\":false}]";
	t.ApplyJsonDiff(json_change3.str());

	// Verify clip reader type
	CHECK(clip1.Reader()->Name() == "QtImageReader");
}

TEST_CASE( "ApplyJSONDiff insert invalidates overlapping timeline cache", "[libopenshot][timeline]" )
{
	// Create timeline with no clips so cached frames are black placeholders
	Timeline t(640, 480, Fraction(30, 1), 44100, 2, LAYOUT_STEREO);
	t.Open();

	// Cache a frame in the area where we'll insert a new clip
	std::shared_ptr<Frame> cached_before = t.GetFrame(10);
	REQUIRE(cached_before != nullptr);
	REQUIRE(t.GetCache() != nullptr);
	REQUIRE(t.GetCache()->Contains(10));

	// Insert clip via JSON diff overlapping frame 10
	std::stringstream path1;
	path1 << TEST_MEDIA_PATH << "interlaced.png";
	std::stringstream json_change;
	json_change << "[{\"type\":\"insert\",\"key\":[\"clips\"],\"value\":{\"id\":\"INSERT_CACHE_INVALIDATE\",\"layer\":1,\"position\":0.0,\"start\":0,\"end\":10,\"reader\":{\"acodec\":\"\",\"audio_bit_rate\":0,\"audio_stream_index\":-1,\"audio_timebase\":{\"den\":1,\"num\":1},\"channel_layout\":4,\"channels\":0,\"display_ratio\":{\"den\":1,\"num\":1},\"duration\":3600.0,\"file_size\":\"160000\",\"fps\":{\"den\":1,\"num\":30},\"has_audio\":false,\"has_single_image\":true,\"has_video\":true,\"height\":200,\"interlaced_frame\":false,\"metadata\":{},\"path\":\"" << path1.str() << "\",\"pixel_format\":-1,\"pixel_ratio\":{\"den\":1,\"num\":1},\"sample_rate\":0,\"top_field_first\":true,\"type\":\"QtImageReader\",\"vcodec\":\"\",\"video_bit_rate\":0,\"video_length\":\"108000\",\"video_stream_index\":-1,\"video_timebase\":{\"den\":30,\"num\":1},\"width\":200}},\"partial\":false}]";
	t.ApplyJsonDiff(json_change.str());

	// Overlapping cached frame should be invalidated
	CHECK(!t.GetCache()->Contains(10));
}

TEST_CASE( "AddClip replaces stale cached black timeline frames with new clip content", "[libopenshot][timeline][cache]" )
{
	Timeline t(640, 480, Fraction(30, 1), 44100, 2, LAYOUT_STEREO);
	t.Open();

	// Cache two frames while the timeline has no clips. This reproduces the
	// stale final_cache state VideoCacheThread can build before a simple edit.
	std::shared_ptr<Frame> cached_before = t.GetFrame(10);
	std::shared_ptr<Frame> cached_far = t.GetFrame(400);
	REQUIRE(cached_before != nullptr);
	REQUIRE(cached_far != nullptr);
	REQUIRE(t.GetCache() != nullptr);
	REQUIRE(t.GetCache()->Contains(10));
	REQUIRE(t.GetCache()->Contains(400));
	CHECK(cached_before->GetImage()->pixelColor(320, 240) == QColor(0, 0, 0, 255));

	TimelineSolidColorReader red_reader(
		/*width=*/640, /*height=*/480, /*fps_num=*/30, /*fps_den=*/1, /*length_frames=*/300,
		QColor(220, 20, 30, 255)
	);
	Clip clip(&red_reader);
	clip.Id("ADDCLIP_CACHE_INVALIDATE");
	clip.Layer(1);
	clip.Position(0.0);
	clip.Start(0.0);
	clip.End(10.0);
	t.AddClip(&clip);

	CHECK(!t.GetCache()->Contains(10));
	CHECK(t.GetCache()->Contains(400));

	std::shared_ptr<Frame> refreshed = t.GetFrame(10);
	REQUIRE(refreshed != nullptr);
	const QColor refreshed_pixel = refreshed->GetImage()->pixelColor(320, 240);
	CHECK(refreshed_pixel.red() == Approx(220).margin(2));
	CHECK(refreshed_pixel.green() == Approx(20).margin(2));
	CHECK(refreshed_pixel.blue() == Approx(30).margin(2));

	t.RemoveClip(&clip);
}

TEST_CASE( "Timeline retries opening intersecting clip after transient open failure", "[libopenshot][timeline]" )
{
	Timeline t(640, 480, Fraction(30, 1), 44100, 2, LAYOUT_STEREO);
	t.Open();
	t.AutoMapClips(false);

	TimelineFailFirstOpenReader reader(
		/*width=*/640, /*height=*/480, /*fps_num=*/30, /*fps_den=*/1, /*length_frames=*/300,
		QColor(30, 210, 40, 255)
	);
	Clip clip(&reader);
	reader.FailNextOpen();
	clip.Id("RETRY_OPEN_AFTER_FAILURE");
	clip.Layer(5000000);
	clip.Position(0.0);
	clip.Start(0.0);
	clip.End(10.0);
	t.AddClip(&clip);

	std::shared_ptr<Frame> failed_open_frame = t.GetFrame(1);
	REQUIRE(failed_open_frame != nullptr);
	CHECK(failed_open_frame->GetImage()->pixelColor(320, 240) == QColor(0, 0, 0, 255));

	t.GetCache()->Clear();
	std::shared_ptr<Frame> retried_frame = t.GetFrame(1);
	REQUIRE(retried_frame != nullptr);
	const QColor retried_pixel = retried_frame->GetImage()->pixelColor(320, 240);
	CHECK(retried_pixel.red() == Approx(30).margin(2));
	CHECK(retried_pixel.green() == Approx(210).margin(2));
	CHECK(retried_pixel.blue() == Approx(40).margin(2));

	t.RemoveClip(&clip);
}

TEST_CASE( "Timeline repairs stale open clip state while clip still intersects playhead", "[libopenshot][timeline][cache]" )
{
	Timeline t(640, 480, Fraction(30, 1), 44100, 2, LAYOUT_STEREO);
	t.Open();

	// Model a long clip whose visible extent is much wider than the editor
	// viewport, with the playhead well inside the clip rather than at an edge.
	TimelineFailFirstOpenReader red_reader(
		/*width=*/640, /*height=*/480, /*fps_num=*/30, /*fps_den=*/1,
		/*length_frames=*/18000, QColor(220, 20, 30, 255)
	);
	Clip clip(&red_reader);
	clip.Id("STALE_OPEN_CLIP_STATE");
	clip.Layer(5);
	clip.Position(0.0);
	clip.Start(0.0);
	clip.End(600.0);
	t.AddClip(&clip);

	const int64_t playhead_frame = 4501; // 150 seconds, deep inside the clip
	std::shared_ptr<Frame> initial = t.GetFrame(playhead_frame);
	REQUIRE(initial != nullptr);
	CHECK(initial->GetImage()->pixelColor(320, 240).red() == Approx(220).margin(2));

	// Reproduce the suspected invariant violation: Timeline::open_clips still
	// contains this Clip pointer and Clip::IsOpen() is still true, but the
	// FrameMapper's nested source Reader has become closed.
	// A timing nudge which continues to intersect the playhead should not leave
	// the preview permanently black.
	REQUIRE(clip.IsOpen());
	auto* mapper = dynamic_cast<FrameMapper*>(clip.Reader());
	REQUIRE(mapper != nullptr);
	mapper->Reader()->Close();
	CHECK(clip.IsOpen());
	CHECK_FALSE(mapper->IsOpen());
	t.GetCache()->Remove(playhead_frame);

	std::stringstream nudge_right;
	nudge_right << "[{\"type\":\"update\",\"key\":[\"clips\",{\"id\":\""
	            << clip.Id()
	            << "\"}],\"value\":{\"id\":\"" << clip.Id()
	            << "\",\"position\":0.1,\"start\":0.0,\"end\":600.0},\"partial\":true}]";
	t.ApplyJsonDiff(nudge_right.str());

	std::shared_ptr<Frame> after_nudge = t.GetFrame(playhead_frame);
	REQUIRE(after_nudge != nullptr);
	const QColor after_nudge_pixel = after_nudge->GetImage()->pixelColor(320, 240);
	CHECK(after_nudge_pixel.red() == Approx(220).margin(2));
	CHECK(after_nudge_pixel.green() == Approx(20).margin(2));
	CHECK(after_nudge_pixel.blue() == Approx(30).margin(2));

	// A second nudge which still covers the playhead must remain healthy after
	// the stale open state was repaired by the first frame request.
	std::stringstream nudge_again;
	nudge_again << "[{\"type\":\"update\",\"key\":[\"clips\",{\"id\":\""
	            << clip.Id()
	            << "\"}],\"value\":{\"id\":\"" << clip.Id()
	            << "\",\"position\":0.2,\"start\":0.0,\"end\":600.0},\"partial\":true}]";
	t.ApplyJsonDiff(nudge_again.str());
	std::shared_ptr<Frame> after_second_nudge = t.GetFrame(playhead_frame);
	REQUIRE(after_second_nudge != nullptr);
	CHECK(after_second_nudge->GetImage()->pixelColor(320, 240).red() == Approx(220).margin(2));

	// Match the UI recovery gesture: move the clip completely past the playhead
	// and render once so update_open_clips() removes its stale registry entry.
	std::stringstream move_past_playhead;
	move_past_playhead << "[{\"type\":\"update\",\"key\":[\"clips\",{\"id\":\""
	                   << clip.Id()
	                   << "\"}],\"value\":{\"id\":\"" << clip.Id()
	                   << "\",\"position\":200.0,\"start\":0.0,\"end\":600.0},\"partial\":true}]";
	t.ApplyJsonDiff(move_past_playhead.str());
	std::shared_ptr<Frame> outside_clip = t.GetFrame(playhead_frame);
	REQUIRE(outside_clip != nullptr);
	CHECK(outside_clip->GetImage()->pixelColor(320, 240) == QColor(0, 0, 0, 255));

	// Moving it back over the same playhead now forces a fresh Clip::Open(), and
	// the source image returns.
	std::stringstream move_back;
	move_back << "[{\"type\":\"update\",\"key\":[\"clips\",{\"id\":\""
	          << clip.Id()
	          << "\"}],\"value\":{\"id\":\"" << clip.Id()
	          << "\",\"position\":0.2,\"start\":0.0,\"end\":600.0},\"partial\":true}]";
	t.ApplyJsonDiff(move_back.str());
	std::shared_ptr<Frame> after_move_back = t.GetFrame(playhead_frame);
	REQUIRE(after_move_back != nullptr);
	const QColor recovered_pixel = after_move_back->GetImage()->pixelColor(320, 240);
	CHECK(recovered_pixel.red() == Approx(220).margin(2));
	CHECK(recovered_pixel.green() == Approx(20).margin(2));
	CHECK(recovered_pixel.blue() == Approx(30).margin(2));

	t.RemoveClip(&clip);
}

TEST_CASE( "ApplyJSONDiff alpha updates refresh fixed-frame preview content", "[libopenshot][timeline]" )
{
	// Deterministic solid-color readers avoid any fixture/image ambiguity.
	TimelineSolidColorReader base_reader(
		/*width=*/64, /*height=*/64, /*fps_num=*/30, /*fps_den=*/1, /*length_frames=*/300,
		QColor(10, 200, 20, 255)
	);
	TimelineSolidColorReader overlay_reader(
		/*width=*/64, /*height=*/64, /*fps_num=*/30, /*fps_den=*/1, /*length_frames=*/300,
		QColor(220, 30, 180, 255)
	);

	Clip base_clip(&base_reader);
	base_clip.Id("BASE_ALPHA_TEST");
	base_clip.Layer(0);
	base_clip.Position(0.0);
	base_clip.End(5.0);

	Clip overlay_clip(&overlay_reader);
	overlay_clip.Id("OVERLAY_ALPHA_TEST");
	overlay_clip.Layer(1);
	overlay_clip.Position(0.0);
	overlay_clip.End(5.0);

	Timeline t(64, 64, Fraction(30, 1), 44100, 2, LAYOUT_STEREO);
	t.AddClip(&base_clip);
	t.AddClip(&overlay_clip);
	t.Open();

	const int64_t frame_number = 1;

	auto apply_alpha = [&](double alpha_value) {
		Json::Value root(Json::arrayValue);
		Json::Value change(Json::objectValue);
		change["type"] = "update";
		change["partial"] = true;

		Json::Value key(Json::arrayValue);
		key.append("clips");
		Json::Value key_id(Json::objectValue);
		key_id["id"] = overlay_clip.Id();
		key.append(key_id);
		change["key"] = key;

		Json::Value alpha_json(Json::objectValue);
		Json::Value points(Json::arrayValue);
		Json::Value p1(Json::objectValue);
		p1["co"]["X"] = 1.0;
		p1["co"]["Y"] = 1.0;
		p1["interpolation"] = 0;
		points.append(p1);
		Json::Value p2(Json::objectValue);
		p2["co"]["X"] = static_cast<double>(frame_number);
		p2["co"]["Y"] = alpha_value;
		p2["interpolation"] = 1;
		points.append(p2);
		alpha_json["Points"] = points;

		Json::Value value(Json::objectValue);
		value["alpha"] = alpha_json;
		change["value"] = value;

		root.append(change);
		t.ApplyJsonDiff(root.toStyledString());

		Clip* updated = t.GetClip(overlay_clip.Id());
		REQUIRE(updated != nullptr);
		CHECK(updated->alpha.GetValue(frame_number) == Approx(alpha_value).margin(0.0001));
	};

	// Establish reference colors for alpha=1.0 (top) and alpha=0.0 (bottom).
	// Prime cache at fixed frame.
	std::shared_ptr<Frame> initial = t.GetFrame(frame_number);
	REQUIRE(initial != nullptr);
	REQUIRE(t.GetCache() != nullptr);
	REQUIRE(t.GetCache()->Contains(frame_number));
	QColor previous_color = initial->GetImage()->pixelColor(20, 20);

	// Repeated alpha updates at the same frame must invalidate the timeline cache
	// and refresh the composited preview content.
	const std::vector<double> alpha_steps = {0.9, 0.8, 0.7, 0.6, 0.5};
	for (double alpha_value : alpha_steps) {
		apply_alpha(alpha_value);
		CHECK(!t.GetCache()->Contains(frame_number));

		// Re-request frame to repopulate the timeline cache before next update.
		std::shared_ptr<Frame> refreshed = t.GetFrame(frame_number);
		REQUIRE(refreshed != nullptr);
		CHECK(t.GetCache()->Contains(frame_number));
		QColor refreshed_color = refreshed->GetImage()->pixelColor(20, 20);
		CHECK(refreshed_color != previous_color);
		previous_color = refreshed_color;
	}
}

TEST_CASE( "ApplyJSONDiff clip Bars effect updates refresh fixed-frame preview content", "[libopenshot][timeline][effect][bars]" )
{
	TimelineSolidColorReader base_reader(
		/*width=*/64, /*height=*/64, /*fps_num=*/30, /*fps_den=*/1, /*length_frames=*/300,
		QColor(10, 200, 20, 255)
	);

	Clip clip(&base_reader);
	clip.Id("BARS_CLIP_TEST");
	clip.Layer(0);
	clip.Position(0.0);
	clip.End(5.0);

	Bars bars;
	bars.Id("BARS_EFFECT_TEST");
	bars.Layer(0);
	bars.Position(0.0);
	bars.Start(0.0);
	bars.End(5.0);
	bars.color = Color("#000000");
	bars.left = Keyframe(0.0);
	bars.top = Keyframe(0.0);
	bars.right = Keyframe(0.0);
	bars.bottom = Keyframe(0.0);
	clip.AddEffect(&bars);

	Timeline t(64, 64, Fraction(30, 1), 44100, 2, LAYOUT_STEREO);
	t.AddClip(&clip);
	t.Open();

	const int64_t frame_number = 1;
	auto frame = t.GetFrame(frame_number);
	REQUIRE(frame != nullptr);
	CHECK(frame->GetImage()->pixelColor(20, 20) == QColor(10, 200, 20, 255));
	uint64_t previous_hash = image_fingerprint(frame->GetImage());

	const std::vector<double> top_steps = {0.02, 0.04, 0.06, 0.08, 0.10};
	for (double top_value : top_steps) {
		Keyframe top_kf(top_value);

		Json::Value root(Json::arrayValue);
		Json::Value change(Json::objectValue);
		change["type"] = "update";
		change["partial"] = true;

		Json::Value key(Json::arrayValue);
		key.append("clips");
		Json::Value clip_key(Json::objectValue);
		clip_key["id"] = clip.Id();
		key.append(clip_key);
		key.append("effects");
		Json::Value effect_key(Json::objectValue);
		effect_key["id"] = bars.Id();
		key.append(effect_key);
		change["key"] = key;

		Json::Value value(Json::objectValue);
		value["top"] = top_kf.JsonValue();
		change["value"] = value;
		root.append(change);

		t.ApplyJsonDiff(root.toStyledString());
		CHECK(bars.top.GetValue(frame_number) == Approx(top_value).margin(0.0001));

		frame = t.GetFrame(frame_number);
		REQUIRE(frame != nullptr);
		const uint64_t current_hash = image_fingerprint(frame->GetImage());

		// Regression check: every Bars update should change the rendered image.
		CHECK(current_hash != previous_hash);
		previous_hash = current_hash;
	}
}

TEST_CASE( "ApplyJSONDiff Update Reader Info", "[libopenshot][timeline]" )
{
	// Create a timeline
	Timeline t(640, 480, Fraction(24, 1), 44100, 2, LAYOUT_STEREO);
	t.Open();

	// Auto create FrameMappers for each clip
	t.AutoMapClips(true);

	// Add clip
	std::stringstream path1;
	path1 << TEST_MEDIA_PATH << "sintel_trailer-720p.mp4";
	Clip clip1(path1.str());
	clip1.Id("ABC");
	clip1.Layer(1);
	clip1.Position(0);
	clip1.End(10);
	std::string reader_json = clip1.Reader()->Json();

	// Verify clip reader type (not wrapped yet, because we have not added clip to timeline)
	CHECK(clip1.Reader()->Name() == "FFmpegReader");

	t.AddClip(&clip1);

	// Verify clip was wrapped in FrameMapper
	CHECK(clip1.Reader()->Name() == "FrameMapper");
	CHECK(clip1.info.fps.num == 24);
	CHECK(clip1.info.fps.den == 1);
	CHECK(clip1.info.video_timebase.num == 1);
	CHECK(clip1.info.video_timebase.den == 24);
	CHECK(clip1.info.duration == Approx(52.20833).margin(0.00001));

	// Create JSON change to increase FPS from 24 to 60
	Json::Value reader_root = openshot::stringToJson(reader_json);
	reader_root["fps"]["num"] = 60;
	reader_root["fps"]["den"] = 1;
	reader_root["video_timebase"]["num"] = 1;
	reader_root["video_timebase"]["den"] = 60;
	reader_root["duration"] = reader_root["duration"].asDouble() * 0.4;
	std::string update_reader = reader_root.toStyledString();

	// Apply JSON changes to clip
	std::stringstream json_change1;
	json_change1 << "[{\"type\":\"update\",\"key\":[\"clips\",{\"id\":\"" << clip1.Id() << "\"}],\"value\":{\"reader\": " << update_reader << "}}]";
	t.ApplyJsonDiff(json_change1.str());

	// Verify clip is still wrapped in FrameMapper
	CHECK(clip1.Reader()->Name() == "FrameMapper");

	// Verify clip Reader has updated properties and info struct
	openshot::FrameMapper* mapper = (openshot::FrameMapper*) clip1.Reader();
	CHECK(mapper->Reader()->info.fps.num == 60);
	CHECK(mapper->Reader()->info.fps.den == 1);
	CHECK(mapper->Reader()->info.video_timebase.num == 1);
	CHECK(mapper->Reader()->info.video_timebase.den == 60);
	CHECK(mapper->Reader()->info.duration == Approx(20.88333).margin(0.00001));

	// Verify clip has updated properties and info struct
	CHECK(clip1.info.fps.num == 24);
	CHECK(clip1.info.fps.den == 1);
	CHECK(clip1.info.video_timebase.num == 1);
	CHECK(clip1.info.video_timebase.den == 24);
	CHECK(clip1.info.duration == Approx(20.88333).margin(0.00001));

	// Open Clip object, and verify this does not clobber our 60 FPS change
	clip1.Open();
	CHECK(mapper->Reader()->info.fps.num == 60);
	CHECK(mapper->Reader()->info.fps.den == 1);
	CHECK(mapper->Reader()->info.video_timebase.num == 1);
	CHECK(mapper->Reader()->info.video_timebase.den == 60);
	CHECK(mapper->Reader()->info.duration == Approx(20.88333).margin(0.00001));

}

TEST_CASE("GetFrame past-end requests are not cached", "[libopenshot][timeline][cache]") {
	TimelineSolidColorReader reader(
		64, 64,
		30, 1,
		300,
		QColor(10, 20, 30, 255));
	Clip clip(&reader);
	clip.Layer(1);
	clip.Position(0.0);
	clip.End(1.0);

	Timeline t(640, 480, Fraction(30, 1), 44100, 2, LAYOUT_STEREO);
	t.AddClip(&clip);
	t.Open();

	const int64_t end = t.GetMaxFrame();
	REQUIRE(end > 1);
	REQUIRE(t.GetCache() != nullptr);
	const int64_t count_before = t.GetCache()->Count();

	std::shared_ptr<Frame> first = t.GetFrame(end + 25);
	REQUIRE(first != nullptr);
	CHECK(first->number == end + 25);
	CHECK(t.GetCache()->Count() == count_before);

	std::shared_ptr<Frame> second = t.GetFrame(end + 120);
	REQUIRE(second != nullptr);
	CHECK(second->number == end + 120);
	CHECK(t.GetCache()->Count() == count_before);
}

TEST_CASE("GetMaxFrame ignores tiny float overshoot at clip end", "[libopenshot][timeline][frames]") {
	TimelineSolidColorReader reader(
		64, 64,
		30, 1,
		600,
		QColor(10, 20, 30, 255));
	Clip clip(&reader);
	clip.Layer(1);
	clip.Position(0.0);
	clip.End(16.83333396911621f);

	Timeline t(640, 480, Fraction(30, 1), 44100, 2, LAYOUT_STEREO);
	t.AddClip(&clip);

	// This value reproduces an exported project where 505 frames at 30 FPS
	// reloaded from JSON as a tiny float overshoot beyond the frame boundary.
	REQUIRE(t.GetMaxTime() * t.info.fps.ToDouble() > 505.0);
	REQUIRE(t.GetMaxTime() * t.info.fps.ToDouble() < 505.0001);
	CHECK(t.GetMaxFrame() == 505);
}

TEST_CASE("JSON transform edits preserve other properties and multiple actions", "[libopenshot][timeline][json]")
{
    DummyReader reader;
    Clip clip(&reader);
    clip.Id("transform-json");
    clip.End(300);
    clip.Layer(5);
    clip.alpha = Keyframe(0.75);
    Timeline timeline(1280, 720, Fraction(24, 1), 44100, 2, LAYOUT_STEREO);
    timeline.AddClip(&clip);
    Json::Value change;
    change["type"] = "update";
    change["key"].append("clips");
    Json::Value id;
    id["id"] = clip.Id();
    change["key"].append(id);
    Keyframe x, y;
    for (int frame = 1; frame <= 1000; ++frame) {
        x.AddPoint(frame, frame * 0.001, LINEAR);
        y.AddPoint(frame, -frame * 0.001, BEZIER);
    }
    change["value"]["location_x"] = x.JsonValue();
    change["value"]["location_y"] = y.JsonValue();
    Json::Value changes(Json::arrayValue);
    changes.append(change);
    Json::Value alpha_change = change;
    alpha_change["value"] = Json::Value(Json::objectValue);
    alpha_change["value"]["alpha"] = Keyframe(0.5).JsonValue();
    changes.append(alpha_change);
    const auto epoch = timeline.CacheEpoch();
    timeline.ApplyJsonDiff(changes.toStyledString());
    CHECK(clip.location_x.JsonValue() == x.JsonValue());
    CHECK(clip.location_y.JsonValue() == y.JsonValue());
    CHECK(clip.alpha.GetValue(1) == Approx(0.5));
    CHECK(clip.Layer() == 5);
    CHECK(clip.End() == Approx(300));
    CHECK(timeline.CacheEpoch() > epoch);
    // A subsequent edit replaces the curve and leaves the other axis intact.
    x.AddPoint(500, 0.9, CONSTANT);
    change["value"].removeMember("location_y");
    change["value"]["location_x"] = x.JsonValue();
    changes.clear();
    changes.append(change);
    timeline.ApplyJsonDiff(changes.toStyledString());
    CHECK(clip.location_x.JsonValue() == x.JsonValue());
    CHECK(clip.location_y.JsonValue() == y.JsonValue());
    CHECK(clip.alpha.GetValue(1) == Approx(0.5));
    timeline.RemoveClip(&clip);
}
