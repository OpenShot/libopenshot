/**
 * @file
 * @brief Unit tests for openshot::Timer
 * @author OpenShot Studios, LLC
 *
 * @ref License
 */

// Copyright (c) 2008-2026 OpenShot Studios, LLC
//
// SPDX-License-Identifier: LGPL-3.0-or-later

#include "openshot_catch.h"

#include <QApplication>
#include <memory>

#include "Clip.h"
#include "DummyReader.h"
#include "EffectInfo.h"
#include "Frame.h"
#include "Json.h"
#include "effects/Timer.h"

static bool HasNonBlackPixels(const std::shared_ptr<openshot::Frame>& frame) {
	for (int row = 0; row < frame->GetHeight(); ++row) {
		const unsigned char* pixels = frame->GetPixels(row);
		for (int col = 0; col < frame->GetWidth(); ++col) {
			const int index = col * 4;
			if (pixels[index] != 0 || pixels[index + 1] != 0 || pixels[index + 2] != 0)
				return true;
		}
	}
	return false;
}

static int FirstNonBlackColumn(const std::shared_ptr<openshot::Frame>& frame) {
	for (int col = 0; col < frame->GetWidth(); ++col) {
		for (int row = 0; row < frame->GetHeight(); ++row) {
			const unsigned char* pixels = frame->GetPixels(row);
			const int index = col * 4;
			if (pixels[index] != 0 || pixels[index + 1] != 0 || pixels[index + 2] != 0)
				return col;
		}
	}
	return -1;
}

static int LastNonBlackColumn(const std::shared_ptr<openshot::Frame>& frame) {
	for (int col = frame->GetWidth() - 1; col >= 0; --col) {
		for (int row = 0; row < frame->GetHeight(); ++row) {
			const unsigned char* pixels = frame->GetPixels(row);
			const int index = col * 4;
			if (pixels[index] != 0 || pixels[index + 1] != 0 || pixels[index + 2] != 0)
				return col;
		}
	}
	return -1;
}

static int FirstNonBlackRow(const std::shared_ptr<openshot::Frame>& frame) {
	for (int row = 0; row < frame->GetHeight(); ++row) {
		const unsigned char* pixels = frame->GetPixels(row);
		for (int col = 0; col < frame->GetWidth(); ++col) {
			const int index = col * 4;
			if (pixels[index] != 0 || pixels[index + 1] != 0 || pixels[index + 2] != 0)
				return row;
		}
	}
	return -1;
}

TEST_CASE("timer effect", "[libopenshot][timer]") {
	int argc = 1;
	char* argv[1] = {(char*)""};
#if QT_VERSION < QT_VERSION_CHECK(6, 0, 0)
	QGuiApplication::setAttribute(Qt::AA_EnableHighDpiScaling);
#endif
	QApplication app(argc, argv);

	SECTION("defaults, json, and properties") {
		openshot::Timer timer;

		CHECK(timer.mode == openshot::TIMER_MODE_COUNT_UP);
		CHECK(timer.time_source == openshot::TIMER_TIME_SOURCE);
		CHECK(timer.format == openshot::TIMER_FORMAT_MM_SS);
		CHECK(timer.clamp == 1);
		CHECK(timer.gravity == openshot::GRAVITY_BOTTOM);
		CHECK(timer.font_name == "sans");
		CHECK(timer.start_time.GetValue(1) == Approx(0.0).margin(0.00001));
		CHECK(timer.end_time.GetValue(1) == Approx(0.0).margin(0.00001));
		CHECK(timer.x_offset.GetValue(1) == Approx(0.0).margin(0.00001));
		CHECK(timer.y_offset.GetValue(1) == Approx(0.0).margin(0.00001));
		CHECK(timer.TimerText(1) == "00:00");

		Json::Value json = timer.JsonValue();
		CHECK(json["type"].asString() == "Timer");
		CHECK(json["gravity"].asInt() == openshot::GRAVITY_BOTTOM);
		CHECK(json["anchor"].isNull());
		CHECK(json["text_align"].isNull());
		json["mode"] = openshot::TIMER_MODE_COUNT_DOWN;
		json["format"] = openshot::TIMER_FORMAT_HH_MM_SS;
		json["prefix"] = "Left ";
		openshot::Timer loaded;
		loaded.SetJsonValue(json);
		CHECK(loaded.mode == openshot::TIMER_MODE_COUNT_DOWN);
		CHECK(loaded.format == openshot::TIMER_FORMAT_HH_MM_SS);
		CHECK(loaded.prefix == "Left ");

		Json::Value properties = openshot::stringToJson(timer.PropertiesJSON(1));
		CHECK(properties["mode"]["name"].asString() == "Mode");
		CHECK(properties["mode"]["choices"].size() == 5);
		CHECK(properties["time_source"]["name"].asString() == "Time Source");
		CHECK(properties["time_source"]["choices"].size() == 2);
		CHECK(properties["gravity"]["name"].asString() == "Gravity");
		CHECK(properties["gravity"]["choices"].size() == 9);
		CHECK(properties["anchor"].isNull());
		CHECK(properties["text_align"].isNull());
		CHECK(properties["x_offset"]["name"].asString() == "X Offset (%)");
		CHECK(properties["x_offset"]["min"].asFloat() == Approx(-100.0).margin(0.00001));
		CHECK(properties["x_offset"]["max"].asFloat() == Approx(100.0).margin(0.00001));
		CHECK(properties["y_offset"]["name"].asString() == "Y Offset (%)");
		CHECK(properties["y_offset"]["min"].asFloat() == Approx(-100.0).margin(0.00001));
		CHECK(properties["y_offset"]["max"].asFloat() == Approx(100.0).margin(0.00001));
		CHECK(properties["show_background"]["choices"].size() == 2);
		CHECK(properties["font_name"]["type"].asString() == "font");
	}

	SECTION("registered in effect catalog and factory") {
		std::unique_ptr<openshot::EffectBase> effect(openshot::EffectInfo().CreateEffect("Timer"));
		REQUIRE(effect);
		CHECK(effect->info.class_name == "Timer");

		Json::Value effects = openshot::EffectInfo().JsonValue();
		bool found_timer = false;
		for (Json::ArrayIndex index = 0; index < effects.size(); ++index) {
			if (effects[index]["class_name"].asString() == "Timer") {
				found_timer = true;
				CHECK(effects[index]["name"].asString() == "Timer");
				break;
			}
		}
		CHECK(found_timer);
	}

	SECTION("formats count up and down") {
		openshot::Timer timer;
		timer.format = openshot::TIMER_FORMAT_HH_MM_SS_MILLISECONDS;
		timer.start_time = openshot::Keyframe(10.0);
		CHECK(timer.TimerText(31) == "00:00:11.000");

		timer.mode = openshot::TIMER_MODE_COUNT_DOWN;
		timer.end_time = openshot::Keyframe(5.0);
		timer.format = openshot::TIMER_FORMAT_MM_SS;
		CHECK(timer.TimerText(151) == "00:00");

		timer.mode = openshot::TIMER_MODE_TIMECODE;
		timer.format = openshot::TIMER_FORMAT_TIMECODE;
		CHECK(timer.TimerText(31) == "00:00:11:00");

		timer.mode = openshot::TIMER_MODE_FRAME_NUMBER;
		timer.format = openshot::TIMER_FORMAT_FRAMES;
		timer.start_time = openshot::Keyframe(1.0);
		CHECK(timer.TimerText(1) == "31");
	}

	SECTION("count down defaults to clip duration") {
		openshot::DummyReader reader(openshot::Fraction(30, 1), 640, 360, 44100, 2, 37.0);
		reader.Open();
		openshot::Clip clip(&reader);
		clip.Open();

		openshot::Timer timer;
		timer.mode = openshot::TIMER_MODE_COUNT_DOWN;
		clip.AddEffect(&timer);

		CHECK(timer.TimerText(1) == "00:37");
		CHECK(timer.TimerText(31) == "00:36");

		timer.start_time = openshot::Keyframe(5.0);
		CHECK(timer.TimerText(1) == "00:32");

		timer.end_time = openshot::Keyframe(60.0);
		CHECK(timer.TimerText(1) == "00:55");

		clip.Close();
		reader.Close();
	}

	SECTION("renders styled timer text") {
		openshot::Timer timer;
		timer.gravity = openshot::GRAVITY_CENTER;
		timer.x_offset = openshot::Keyframe(0.0);
		timer.y_offset = openshot::Keyframe(0.0);
		timer.font_size = openshot::Keyframe(36.0);

		auto frame = std::make_shared<openshot::Frame>(1, 640, 360, "#000000", 0, 2);
		timer.GetFrame(frame, 1);
		CHECK(HasNonBlackPixels(frame));
	}

	SECTION("gravity affects rendered placement") {
		openshot::Timer left_timer;
		left_timer.gravity = openshot::GRAVITY_LEFT;
		left_timer.x_offset = openshot::Keyframe(0.0);
		left_timer.y_offset = openshot::Keyframe(0.0);
		left_timer.font_size = openshot::Keyframe(36.0);
		left_timer.stroke_width = openshot::Keyframe(0.0);
		left_timer.show_background = 0;

		openshot::Timer right_timer = left_timer;
		right_timer.gravity = openshot::GRAVITY_RIGHT;

		auto left_frame = std::make_shared<openshot::Frame>(1, 640, 360, "#000000", 0, 2);
		auto right_frame = std::make_shared<openshot::Frame>(1, 640, 360, "#000000", 0, 2);
		left_timer.GetFrame(left_frame, 1);
		right_timer.GetFrame(right_frame, 1);

		const int left_column = FirstNonBlackColumn(left_frame);
		const int right_column = FirstNonBlackColumn(right_frame);
		REQUIRE(left_column >= 0);
		REQUIRE(right_column >= 0);
		CHECK(right_column > left_column + 300);
	}

	SECTION("centered countdown uses stable layout width") {
		openshot::Timer timer;
		timer.mode = openshot::TIMER_MODE_COUNT_DOWN;
		timer.time_source = openshot::TIMER_TIME_CLIP;
		timer.end_time = openshot::Keyframe(11.0);
		timer.gravity = openshot::GRAVITY_BOTTOM;
		timer.font_size = openshot::Keyframe(48.0);
		timer.stroke_width = openshot::Keyframe(0.0);
		timer.show_background = 0;

		auto wide_frame = std::make_shared<openshot::Frame>(1, 640, 360, "#000000", 0, 2);
		auto narrow_frame = std::make_shared<openshot::Frame>(331, 640, 360, "#000000", 0, 2);
		timer.GetFrame(wide_frame, 1);
		timer.GetFrame(narrow_frame, 331);

		const int wide_left = FirstNonBlackColumn(wide_frame);
		const int wide_right = LastNonBlackColumn(wide_frame);
		const int narrow_left = FirstNonBlackColumn(narrow_frame);
		const int narrow_right = LastNonBlackColumn(narrow_frame);
		REQUIRE(wide_left >= 0);
		REQUIRE(wide_right >= 0);
		REQUIRE(narrow_left >= 0);
		REQUIRE(narrow_right >= 0);

		const double wide_center = (wide_left + wide_right) / 2.0;
		const double narrow_center = (narrow_left + narrow_right) / 2.0;
		CHECK(narrow_center == Approx(wide_center).margin(3.0));
	}

	SECTION("offsets are percentages of the frame") {
		openshot::Timer base_timer;
		base_timer.gravity = openshot::GRAVITY_CENTER;
		base_timer.font_size = openshot::Keyframe(36.0);
		base_timer.stroke_width = openshot::Keyframe(0.0);
		base_timer.show_background = 0;

		openshot::Timer shifted_timer = base_timer;
		shifted_timer.x_offset = openshot::Keyframe(10.0);
		shifted_timer.y_offset = openshot::Keyframe(10.0);

		auto base_frame = std::make_shared<openshot::Frame>(1, 640, 360, "#000000", 0, 2);
		auto shifted_frame = std::make_shared<openshot::Frame>(1, 640, 360, "#000000", 0, 2);
		base_timer.GetFrame(base_frame, 1);
		shifted_timer.GetFrame(shifted_frame, 1);

		const int base_column = FirstNonBlackColumn(base_frame);
		const int shifted_column = FirstNonBlackColumn(shifted_frame);
		const int base_row = FirstNonBlackRow(base_frame);
		const int shifted_row = FirstNonBlackRow(shifted_frame);
		REQUIRE(base_column >= 0);
		REQUIRE(shifted_column >= 0);
		REQUIRE(base_row >= 0);
		REQUIRE(shifted_row >= 0);
		CHECK(shifted_column > base_column + 50);
		CHECK(shifted_row > base_row + 25);
	}

	SECTION("source time follows clip time keyframe") {
		openshot::DummyReader reader(openshot::Fraction(30, 1), 640, 360, 44100, 2, 10.0);
		reader.Open();
		openshot::Clip clip(&reader);
		clip.Open();
		clip.time.AddPoint(1, 1.0);
		clip.time.AddPoint(31, 16.0);

		openshot::Timer timer;
		timer.time_source = openshot::TIMER_TIME_SOURCE;
		clip.AddEffect(&timer);

		CHECK(timer.TimerSeconds(31) == Approx(0.5).margin(0.00001));
		CHECK(timer.TimerText(31) == "00:00");

		clip.Close();
		reader.Close();
	}

	app.quit();
}
