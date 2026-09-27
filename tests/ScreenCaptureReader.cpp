/**
 * @file
 * @brief Unit tests for openshot::ScreenCaptureReader settings and metadata
 * @author Jonathan Thomas <jonathan@openshot.org>
 *
 * @ref License
 */

// Copyright (c) 2008-2026 OpenShot Studios, LLC
//
// SPDX-License-Identifier: LGPL-3.0-or-later

#include "openshot_catch.h"

#include "Exceptions.h"
#include "CaptureAudioBuffer.h"
#include "ScreenCaptureReader.h"
#include "WaylandBufferUtilities.h"
#include "FFmpegColorRange.h"

#include <cstdlib>
#include <cstdint>
#include <string>
#include <vector>

using namespace openshot;

TEST_CASE("JPEG capture conversion preserves full-range black and white",
          "[libopenshot][screencapturereader][color-range]")
{
	for (auto format : {AV_PIX_FMT_YUVJ420P, AV_PIX_FMT_YUVJ422P,
		AV_PIX_FMT_YUVJ444P, AV_PIX_FMT_YUVJ440P}) {
		bool full_range = false;
		const auto normalized = NormalizeDeprecatedPixFmt(format, full_range);
		CHECK(full_range);
		CHECK(normalized != format);
		uint8_t* source[4] = {};
		int strides[4] = {};
		const int size = av_image_alloc(source, strides, 16, 16, normalized, 32);
		REQUIRE(size > 0);
		memset(source[0], 128, size);
		uint8_t* dest[4] = {};
		int dest_strides[4] = {};
		REQUIRE(av_image_alloc(dest, dest_strides, 16, 16, AV_PIX_FMT_RGBA, 32) > 0);
		SwsContext* context = sws_getContext(16, 16, normalized, 16, 16,
			AV_PIX_FMT_RGBA, SWS_BILINEAR, nullptr, nullptr, nullptr);
		REQUIRE(context != nullptr);
		const int* coefficients = sws_getCoefficients(SWS_CS_DEFAULT);
		REQUIRE(sws_setColorspaceDetails(context, coefficients, full_range, coefficients,
			1, 0, 1 << 16, 1 << 16) >= 0);
		for (int level : {0, 16, 235, 255}) {
			for (int row = 0; row < 16; ++row)
				memset(source[0] + row * strides[0], level, 16);
			REQUIRE(sws_scale(context, source, strides, 0, 16, dest, dest_strides) == 16);
			for (int channel = 0; channel < 3; ++channel)
				CHECK(int(dest[0][channel]) == Approx(level).margin(2));
		}
		sws_freeContext(context);
		av_freep(&source[0]);
		av_freep(&dest[0]);
	}
}

TEST_CASE("PulseAudio and WASAPI clocks align to the same recording samples",
	"[libopenshot][screencapturereader][audio][sync]")
{
	for (int ticks_per_second : {1000000, 10000000}) {
		for (int rate : {44100, 48000}) {
			CaptureAudioBuffer buffer;
			buffer.Reset(2, rate * 10);
			const int64_t epoch = int64_t{123456789} * ticks_per_second;
			// Device delivers a packet spanning recording start, followed by a
			// gap and a new sound. Arrival time must not define either position.
			buffer.PushTimestamp(epoch - ticks_per_second / 10, epoch, {1, ticks_per_second},
				rate, {std::vector<float>(rate / 5, 1), std::vector<float>(rate / 5, 1)});
			buffer.PushTimestamp(epoch + ticks_per_second / 5, epoch, {1, ticks_per_second},
				rate, {std::vector<float>(rate / 10, 2), std::vector<float>(rate / 10, 2)});
			CHECK(buffer.Read(rate / 10)[0] == std::vector<float>(rate / 10, 1));
			CHECK(buffer.Read(rate / 10)[0] == std::vector<float>(rate / 10, 0));
			CHECK(buffer.Read(rate / 10)[0] == std::vector<float>(rate / 10, 2));
			CHECK(buffer.Read(rate / 10)[0] == std::vector<float>(rate / 10, 0));
		}
	}
}

TEST_CASE("Capture timestamp jitter does not cut or pad adjacent PCM packets",
	"[libopenshot][screencapturereader][audio][sync]")
{
	for (int ticks_per_second : {1000000, 10000000}) {
		CaptureAudioBuffer buffer;
		buffer.Reset(1, 480000);
		const int64_t epoch = int64_t{123456789} * ticks_per_second;
		for (int packet = 0; packet < 200; ++packet) {
			std::vector<float> samples(960);
			for (int i = 0; i < 960; ++i) samples[i] = float(packet * 960 + i + 1);
			const int jitter = packet == 0 ? 0 : (packet % 13) - 6;
			const int64_t timestamp = epoch + av_rescale(packet * 960 + jitter, ticks_per_second, 48000);
			buffer.PushTimestamp(timestamp, epoch, {1, ticks_per_second}, 48000, {samples});
			// Consume every packet: continuity must survive an empty queue too.
			CHECK(buffer.Read(960)[0] == samples);
		}
		// A genuine 20 ms gap is retained, rather than appended to prior audio.
		buffer.PushTimestamp(epoch + int64_t{ticks_per_second} * 402 / 100, epoch,
			{1, ticks_per_second}, 48000, {{7.0f}});
		CHECK(buffer.Read(960)[0] == std::vector<float>(960, 0.0f));
		CHECK(buffer.Read(1)[0] == std::vector<float>{7.0f});
		buffer.Reset(1, 480000);
		buffer.PushTimestamp(epoch, epoch, {1, ticks_per_second}, 48000, {{9.0f}});
		CHECK(buffer.Read(1)[0] == std::vector<float>{9.0f});
	}
}

TEST_CASE("System audio buffered before recording does not delay the recorded stop",
	"[libopenshot][screencapturereader][audio][sync]")
{
	// 1 kHz makes sample indices milliseconds. The source delivers two seconds
	// of pre-recording audio followed by a tone at 200-299 ms of the recording.
	CaptureAudioBuffer buffer;
	buffer.Reset(2, 10000);
	std::vector<float> captured(2500, 0.0f);
	std::fill(captured.begin() + 2200, captured.begin() + 2300, 1.0f);
	buffer.Push(-2000, {captured, captured});
	const auto audio = buffer.Read(500);
	REQUIRE(audio.size() == 2);
	for (const auto& channel : audio) {
		CHECK(std::all_of(channel.begin(), channel.begin() + 200, [](float x) { return x == 0.0f; }));
		CHECK(std::all_of(channel.begin() + 200, channel.begin() + 300, [](float x) { return x == 1.0f; }));
		CHECK(std::all_of(channel.begin() + 300, channel.end(), [](float x) { return x == 0.0f; }));
	}
}

TEST_CASE("Late system audio never shifts samples beyond an already written gap",
	"[libopenshot][screencapturereader][audio][sync]")
{
	CaptureAudioBuffer buffer;
	buffer.Reset(1, 1000);
	CHECK_FALSE(buffer.Covers(100));
	CHECK(buffer.Read(100)[0] == std::vector<float>(100, 0.0f));
	// First packet arrives after a timeout. Only its unwritten half is usable.
	std::vector<float> packet(200, 0.0f);
	packet[150] = 1.0f;
	buffer.Push(0, {packet});
	CHECK(buffer.Covers(100));
	const auto audio = buffer.Read(100);
	CHECK(audio[0][50] == 1.0f);
	buffer.Push(0, {std::vector<float>(100, 1.0f)});
	CHECK(buffer.Read(100)[0] == std::vector<float>(100, 0.0f));
}

TEST_CASE("System audio gaps and queue overflow preserve sample positions",
	"[libopenshot][screencapturereader][audio][sync]")
{
	CaptureAudioBuffer buffer;
	buffer.Reset(1, 100);
	buffer.Push(0, {std::vector<float>(100, 1.0f)});
	buffer.Push(200, {std::vector<float>(100, 2.0f)});
	const auto audio = buffer.Read(300);
	CHECK(std::all_of(audio[0].begin(), audio[0].begin() + 200, [](float x) { return x == 0.0f; }));
	CHECK(std::all_of(audio[0].begin() + 200, audio[0].end(), [](float x) { return x == 2.0f; }));
	buffer.Reset(1, 100);
	CHECK_FALSE(buffer.Covers(1));
	buffer.Push(-20, {std::vector<float>(10, 1.0f)});
	buffer.Push(50, {{3.0f}});
	CHECK(buffer.Read(51)[0][50] == 3.0f);
}

TEST_CASE("System audio packet delivery boundaries do not alter the timeline",
	"[libopenshot][screencapturereader][audio][sync]")
{
	CaptureAudioBuffer buffer;
	buffer.Reset(1, 100);
	buffer.Push(4, {{5, 6, 7, 8}});
	buffer.Push(0, {{1, 2, 3, 4}});
	CHECK(buffer.Read(3)[0] == std::vector<float>{1, 2, 3});
	CHECK(buffer.Read(3)[0] == std::vector<float>{4, 5, 6});
	CHECK(buffer.Read(3)[0] == std::vector<float>{7, 8, 0});
}

TEST_CASE("Wayland packed video layout clamps unsafe PipeWire metadata",
	"[libopenshot][screencapturereader][wayland]")
{
	SECTION("chunk offsets follow PipeWire modulo semantics")
	{
		const auto layout = wayland::ResolvePackedVideoLayout(
			32, 37, 32, 16, 4, 2, 0, 0, 4, 2);
		REQUIRE(layout.valid);
		CHECK(layout.offset == 5);
		CHECK(layout.valid_size == 32);
		CHECK(layout.width == 4);
		CHECK(layout.height == 2);
	}

	SECTION("chunk size limits readable rows")
	{
		const auto layout = wayland::ResolvePackedVideoLayout(
			64, 0, 32, 16, 4, 4, 0, 0, 4, 4);
		REQUIRE(layout.valid);
		CHECK(layout.width == 4);
		CHECK(layout.height == 2);
	}

	SECTION("empty chunks are rejected instead of reading stale allocation data")
	{
		const auto layout = wayland::ResolvePackedVideoLayout(
			64, 0, 0, 16, 4, 4, 0, 0, 4, 4);
		CHECK_FALSE(layout.valid);
	}

	SECTION("crop width cannot exceed row stride")
	{
		const auto layout = wayland::ResolvePackedVideoLayout(
			64, 0, 64, 16, 8, 4, 2, 0, 6, 4);
		REQUIRE(layout.valid);
		CHECK(layout.crop_x == 2);
		CHECK(layout.width == 2);
		CHECK(layout.height == 4);
	}

	SECTION("crop outside readable memory is rejected")
	{
		const auto layout = wayland::ResolvePackedVideoLayout(
			64, 0, 64, 16, 8, 4, 4, 0, 4, 4);
		CHECK_FALSE(layout.valid);
	}

	SECTION("negative producer stride is rejected safely")
	{
		const auto layout = wayland::ResolvePackedVideoLayout(
			64, 0, 64, -16, 4, 4, 0, 0, 4, 4);
		CHECK_FALSE(layout.valid);
	}
}

TEST_CASE("Wayland packed video rows copy safely across ring-buffer wrap",
	"[libopenshot][screencapturereader][wayland]")
{
	const std::vector<uint8_t> source {0, 1, 2, 3, 4, 5, 6, 7};
	std::vector<uint8_t> destination(6, 0);

	REQUIRE(wayland::CopyWrappedBytes(
		source.data(), source.size(), 6, 0,
		destination.data(), destination.size()));
	CHECK(destination == std::vector<uint8_t> {6, 7, 0, 1, 2, 3});

	CHECK_FALSE(wayland::CopyWrappedBytes(
		source.data(), source.size(), 0, 0,
		destination.data(), source.size() + 1));
}

TEST_CASE("Wayland damage-driven streams repeat at the requested cadence",
	"[libopenshot][screencapturereader][wayland]")
{
	CHECK(wayland::DamageFrameWaitMilliseconds(30, 1, false) == 5000);
	CHECK(wayland::DamageFrameWaitMilliseconds(30, 1, true) == 33);
	CHECK(wayland::DamageFrameWaitMilliseconds(60, 1, true) == 16);
	CHECK(wayland::DamageFrameWaitMilliseconds(0, 0, true) == 33);
}

TEST_CASE("Screen capture settings validation", "[libopenshot][screencapturereader]")
{
	ScreenCaptureSettings settings;
#if defined(__linux__)
	settings.backend = SCREEN_CAPTURE_X11;
	settings.display = ":0.0";
#elif defined(_WIN32)
	settings.backend = SCREEN_CAPTURE_WINDOWS_GDI;
	settings.display = "desktop";
#elif defined(__APPLE__)
	settings.backend = SCREEN_CAPTURE_MAC_AVFOUNDATION;
	settings.display = "Capture screen 0:none";
#endif
#if defined(__linux__) || defined(_WIN32) || defined(__APPLE__)
	settings.width = 640;
	settings.height = 360;
	settings.fps = Fraction(30, 1);

	CHECK_NOTHROW([&settings]() { ScreenCaptureReader reader(settings); }());

	settings.width = 0;
	CHECK_THROWS_AS([&settings]() { ScreenCaptureReader reader(settings); }(), InvalidOptions);

	settings.width = 640;
	settings.fps = Fraction(0, 1);
	CHECK_THROWS_AS([&settings]() { ScreenCaptureReader reader(settings); }(), InvalidOptions);
#else
	CHECK_FALSE(ScreenCaptureReader::IsBackendSupported(SCREEN_CAPTURE_X11));
#endif
}

TEST_CASE("Screen capture reader reports configured video info", "[libopenshot][screencapturereader]")
{
	ScreenCaptureSettings settings;
#if defined(__linux__)
	settings.backend = SCREEN_CAPTURE_X11;
	settings.display = ":99.0";
#elif defined(_WIN32)
	settings.backend = SCREEN_CAPTURE_WINDOWS_GDI;
	settings.display = "desktop";
#elif defined(__APPLE__)
	settings.backend = SCREEN_CAPTURE_MAC_AVFOUNDATION;
	settings.display = "Capture screen 0:none";
#endif
#if defined(__linux__) || defined(_WIN32) || defined(__APPLE__)
	settings.x = 10;
	settings.y = 20;
	settings.width = 800;
	settings.height = 450;
	settings.fps = Fraction(25, 1);
	settings.include_cursor = false;
	settings.show_region = true;
	settings.options["window_id"] = "12345";

	ScreenCaptureReader reader(settings);
	CHECK(reader.Name() == "ScreenCaptureReader");
	CHECK(reader.info.has_video == true);
	CHECK(reader.info.has_audio == false);
	CHECK(reader.info.width == 800);
	CHECK(reader.info.height == 450);
	CHECK(reader.info.fps.num == 25);
	CHECK(reader.info.fps.den == 1);

	const Json::Value json = reader.JsonValue();
	CHECK(json["type"].asString() == "ScreenCaptureReader");
#if defined(__APPLE__)
	CHECK(json["display"].asString() == "Capture screen 0:none");
#elif defined(_WIN32)
	CHECK(json["display"].asString() == "desktop");
#else
	CHECK(json["display"].asString() == ":99.0");
#endif
	CHECK(json["x"].asInt() == 10);
	CHECK(json["y"].asInt() == 20);
	CHECK(json["include_cursor"].asBool() == false);
	CHECK(json["show_region"].asBool() == true);
#if defined(__linux__)
	CHECK(json["options"]["window_id"].asString() == "12345");
#endif
#else
	CHECK_FALSE(ScreenCaptureReader::IsBackendSupported(SCREEN_CAPTURE_X11));
#endif
}

TEST_CASE("Closed screen capture reader consistently rejects frames", "[libopenshot][screencapturereader][lifecycle]")
{
	ScreenCaptureSettings settings;
#if defined(__linux__)
	settings.backend = SCREEN_CAPTURE_X11;
	settings.display = ":99.0";
#elif defined(_WIN32)
	settings.backend = SCREEN_CAPTURE_WINDOWS_GDI;
	settings.display = "desktop";
#elif defined(__APPLE__)
	settings.backend = SCREEN_CAPTURE_MAC_AVFOUNDATION;
	settings.display = "Capture screen 0:none";
#else
	return;
#endif
	settings.width = 640;
	settings.height = 360;
	settings.fps = Fraction(30, 1);

	ScreenCaptureReader reader(settings);
	CHECK_FALSE(reader.IsOpen());
	CHECK_NOTHROW(reader.Close());
	CHECK_NOTHROW(reader.Close());
	CHECK_FALSE(reader.IsOpen());
	CHECK_THROWS_AS(reader.GetFrame(1), ReaderClosed);
}

TEST_CASE("Screen capture system audio settings follow backend capability", "[libopenshot][screencapturereader][audio]")
{
	ScreenCaptureSettings settings;
#if defined(__linux__)
	settings.backend = SCREEN_CAPTURE_X11;
	settings.display = ":99.0";
#elif defined(_WIN32)
	settings.backend = SCREEN_CAPTURE_WINDOWS_GDI;
	settings.display = "desktop";
#elif defined(__APPLE__)
	settings.backend = SCREEN_CAPTURE_MAC_AVFOUNDATION;
	settings.display = "Capture screen 0:none";
#else
	return;
#endif
	settings.width = 640;
	settings.height = 360;
	settings.capture_audio = true;
	settings.audio_device = "test-output";
	settings.audio_sample_rate = 48000;
	settings.audio_channels = 2;

	if (ScreenCaptureReader::IsSystemAudioSupported(settings.backend)) {
		ScreenCaptureReader reader(settings);
		CHECK(reader.info.has_audio);
		CHECK(reader.info.sample_rate == 48000);
		CHECK(reader.info.channels == 2);
		CHECK(reader.info.channel_layout == LAYOUT_STEREO);
		const Json::Value json = reader.JsonValue();
		CHECK(json["capture_audio"].asBool());
		CHECK(json["audio_device"].asString() == "test-output");
		CHECK(json["audio_sample_rate"].asInt() == 48000);
		CHECK(json["audio_channels"].asInt() == 2);
	} else {
		CHECK_THROWS_AS([&settings]() { ScreenCaptureReader reader(settings); }(), InvalidOptions);
	}
}

TEST_CASE("Screen capture rejects invalid system audio formats", "[libopenshot][screencapturereader][audio]")
{
	ScreenCaptureSettings settings;
#if defined(__linux__)
	settings.backend = SCREEN_CAPTURE_X11;
	settings.display = ":99.0";
#elif defined(_WIN32)
	settings.backend = SCREEN_CAPTURE_WINDOWS_GDI;
	settings.display = "desktop";
#elif defined(__APPLE__)
	settings.backend = SCREEN_CAPTURE_MAC_AVFOUNDATION;
	settings.display = "Capture screen 0:none";
#else
	return;
#endif
	settings.audio_sample_rate = 7999;
	CHECK_THROWS_AS([&settings]() { ScreenCaptureReader reader(settings); }(), InvalidSampleRate);
	settings.audio_sample_rate = 48000;
	settings.audio_channels = 3;
	CHECK_THROWS_AS([&settings]() { ScreenCaptureReader reader(settings); }(), InvalidChannels);
}

TEST_CASE("Screen capture backend support follows platform build features", "[libopenshot][screencapturereader]")
{
#if defined(__linux__)
	CHECK(ScreenCaptureReader::IsBackendSupported(SCREEN_CAPTURE_AUTO));
	CHECK(ScreenCaptureReader::IsBackendSupported(SCREEN_CAPTURE_X11));

	ScreenCaptureSettings wayland_settings;
	wayland_settings.backend = SCREEN_CAPTURE_WAYLAND;
	wayland_settings.width = 640;
	wayland_settings.height = 360;

	if (ScreenCaptureReader::IsBackendSupported(SCREEN_CAPTURE_WAYLAND)) {
		CHECK_NOTHROW([&wayland_settings]() { ScreenCaptureReader reader(wayland_settings); }());
	} else {
		CHECK_THROWS_AS([&wayland_settings]() { ScreenCaptureReader reader(wayland_settings); }(), InvalidOptions);
	}
#else
#if defined(_WIN32)
	CHECK(ScreenCaptureReader::IsBackendSupported(SCREEN_CAPTURE_AUTO));
	CHECK(ScreenCaptureReader::IsBackendSupported(SCREEN_CAPTURE_WINDOWS_GDI));
#elif defined(__APPLE__)
	CHECK(ScreenCaptureReader::IsBackendSupported(SCREEN_CAPTURE_AUTO));
	CHECK(ScreenCaptureReader::IsBackendSupported(SCREEN_CAPTURE_MAC_AVFOUNDATION));
#else
	CHECK_FALSE(ScreenCaptureReader::IsBackendSupported(SCREEN_CAPTURE_AUTO));
#endif
	CHECK_FALSE(ScreenCaptureReader::IsBackendSupported(SCREEN_CAPTURE_X11));
	CHECK_FALSE(ScreenCaptureReader::IsBackendSupported(SCREEN_CAPTURE_WAYLAND));
#endif
}

TEST_CASE("Screen capture default backend follows platform", "[libopenshot][screencapturereader]")
{
#if defined(_WIN32)
	CHECK(ScreenCaptureReader::DefaultBackend() == SCREEN_CAPTURE_WINDOWS_GDI);
#elif defined(__APPLE__)
	CHECK(ScreenCaptureReader::DefaultBackend() == SCREEN_CAPTURE_MAC_AVFOUNDATION);
#endif
}

TEST_CASE("Screen capture auto backend prefers Wayland only when supported", "[libopenshot][screencapturereader]")
{
#if defined(__linux__)
	const char* previous_session = std::getenv("XDG_SESSION_TYPE");
	const std::string previous_value = previous_session ? previous_session : "";
	setenv("XDG_SESSION_TYPE", "wayland", 1);

	const ScreenCaptureBackend default_backend = ScreenCaptureReader::DefaultBackend();
	if (ScreenCaptureReader::IsBackendSupported(SCREEN_CAPTURE_WAYLAND)) {
		CHECK(default_backend == SCREEN_CAPTURE_WAYLAND);
	} else {
		CHECK(default_backend == SCREEN_CAPTURE_AUTO);
	}

	if (previous_session) {
		setenv("XDG_SESSION_TYPE", previous_value.c_str(), 1);
	} else {
		unsetenv("XDG_SESSION_TYPE");
	}
#endif
}
