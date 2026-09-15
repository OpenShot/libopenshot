/**
 * @file
 * @brief Unit tests for openshot::CameraCaptureReader settings and metadata
 * @author Jonathan Thomas <jonathan@openshot.org>
 *
 * @ref License
 */

// Copyright (c) 2008-2026 OpenShot Studios, LLC
//
// SPDX-License-Identifier: LGPL-3.0-or-later

#include "openshot_catch.h"

#include "CameraCaptureReader.h"
#include "Exceptions.h"

#include <chrono>
#include <condition_variable>
#include <future>
#include <mutex>
#include <thread>

extern "C" {
#include <libavutil/log.h>
}

using namespace openshot;

#if defined(__linux__)
namespace {
// Pause a real V4L2 open at its error log, while FFmpeg still owns the input
// context. This requires no camera hardware and makes the cleanup race repeatable.
struct CameraOpenGate {
	std::mutex mutex;
	std::condition_variable changed;
	bool entered = false;
	bool release = false;
	static thread_local CameraOpenGate* active;

	static void Log(void* context, int level, const char* format, va_list args)
	{
		if (!active) {
			av_log_default_callback(context, level, format, args);
			return;
		}
		std::unique_lock<std::mutex> lock(active->mutex);
		active->entered = true;
		active->changed.notify_all();
		active->changed.wait(lock, [] { return active->release; });
	}
};
thread_local CameraOpenGate* CameraOpenGate::active = nullptr;
}

TEST_CASE("Camera close waits for in-flight FFmpeg initialization",
	"[libopenshot][cameracapturereader][lifecycle]")
{
	CameraCaptureSettings settings;
	settings.backend = CAMERA_CAPTURE_V4L2;
	// /dev/null is a file, so this path cannot accidentally name a real camera.
	settings.device = "/dev/null/openshot-test-camera";
	CameraCaptureReader reader(settings);
	CameraOpenGate gate;
	std::exception_ptr open_error;
	av_log_set_callback(CameraOpenGate::Log);
	std::thread opener([&] {
		CameraOpenGate::active = &gate;
		try { reader.Open(); } catch (...) { open_error = std::current_exception(); }
		CameraOpenGate::active = nullptr;
	});
	bool entered;
	{
		std::unique_lock<std::mutex> lock(gate.mutex);
		entered = gate.changed.wait_for(lock, std::chrono::seconds(5), [&] { return gate.entered; });
	}
	std::promise<void> close_started;
	std::promise<void> close_done;
	auto done = close_done.get_future();
	std::thread closer([&] {
		close_started.set_value();
		reader.Close();
		close_done.set_value();
	});
	close_started.get_future().wait();
	const bool cleanup_waited = done.wait_for(std::chrono::milliseconds(100)) == std::future_status::timeout;
	{
		std::lock_guard<std::mutex> lock(gate.mutex);
		gate.release = true;
	}
	gate.changed.notify_all();
	opener.join();
	closer.join();
	av_log_set_callback(av_log_default_callback);
	REQUIRE(entered);
	CHECK(cleanup_waited);
	CHECK(open_error != nullptr);
	CHECK_FALSE(reader.IsOpen());
	CHECK_NOTHROW(reader.Close());
	// A failed/cancelled open must leave the object safe for another attempt.
	CHECK_THROWS_AS(reader.Open(), InvalidFile);
	CHECK_FALSE(reader.IsOpen());
}
#endif

TEST_CASE("Camera capture settings validation", "[libopenshot][cameracapturereader]")
{
	CameraCaptureSettings settings;
#if defined(__linux__)
	settings.backend = CAMERA_CAPTURE_V4L2;
	settings.device = "/dev/video0";
#elif defined(_WIN32)
	settings.backend = CAMERA_CAPTURE_WINDOWS_DSHOW;
	settings.device = "Integrated Camera";
#elif defined(__APPLE__)
	settings.backend = CAMERA_CAPTURE_MAC_AVFOUNDATION;
	settings.device = "0:none";
#endif
#if defined(__linux__) || defined(_WIN32) || defined(__APPLE__)
	settings.width = 640;
	settings.height = 480;
	settings.fps = Fraction(30, 1);

	CHECK_NOTHROW([&settings]() { CameraCaptureReader reader(settings); }());

	settings.device = "";
	CHECK_THROWS_AS([&settings]() { CameraCaptureReader reader(settings); }(), InvalidOptions);

	settings.device = "/dev/video0";
	settings.height = 0;
	CHECK_THROWS_AS([&settings]() { CameraCaptureReader reader(settings); }(), InvalidOptions);
#else
	CHECK_FALSE(CameraCaptureReader::IsBackendSupported(CAMERA_CAPTURE_V4L2));
#endif
}

TEST_CASE("Camera capture reader reports configured video info", "[libopenshot][cameracapturereader]")
{
	CameraCaptureSettings settings;
#if defined(__linux__)
	settings.backend = CAMERA_CAPTURE_V4L2;
	settings.device = "/dev/video9";
#elif defined(_WIN32)
	settings.backend = CAMERA_CAPTURE_WINDOWS_DSHOW;
	settings.device = "Integrated Camera";
#elif defined(__APPLE__)
	settings.backend = CAMERA_CAPTURE_MAC_AVFOUNDATION;
	settings.device = "0:none";
#endif
#if defined(__linux__) || defined(_WIN32) || defined(__APPLE__)
	settings.width = 1280;
	settings.height = 720;
	settings.fps = Fraction(24, 1);

	CameraCaptureReader reader(settings);
	CHECK(reader.Name() == "CameraCaptureReader");
	CHECK(reader.info.has_video == true);
	CHECK(reader.info.has_audio == false);
	CHECK(reader.info.width == 1280);
	CHECK(reader.info.height == 720);
	CHECK(reader.info.fps.num == 24);

	const Json::Value json = reader.JsonValue();
	CHECK(json["type"].asString() == "CameraCaptureReader");
#if defined(__linux__)
	CHECK(json["device"].asString() == "/dev/video9");
#elif defined(_WIN32)
	CHECK(json["device"].asString() == "Integrated Camera");
#elif defined(__APPLE__)
	CHECK(json["device"].asString() == "0:none");
#endif
	CHECK(json["width"].asInt() == 1280);
	CHECK(json["height"].asInt() == 720);
#else
	CHECK_FALSE(CameraCaptureReader::IsBackendSupported(CAMERA_CAPTURE_V4L2));
#endif
}

TEST_CASE("Camera capture default backend follows platform", "[libopenshot][cameracapturereader]")
{
#if defined(_WIN32)
	CHECK(CameraCaptureReader::IsBackendSupported(CAMERA_CAPTURE_WINDOWS_DSHOW));
	CHECK(CameraCaptureReader::DefaultBackend() == CAMERA_CAPTURE_WINDOWS_DSHOW);
#elif defined(__APPLE__)
	CHECK(CameraCaptureReader::IsBackendSupported(CAMERA_CAPTURE_MAC_AVFOUNDATION));
	CHECK(CameraCaptureReader::DefaultBackend() == CAMERA_CAPTURE_MAC_AVFOUNDATION);
#endif
}
