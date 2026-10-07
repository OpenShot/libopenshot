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
#include "Frame.h"

#include <chrono>
#include <cstdlib>
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

#if defined(__linux__)
#include "CameraCaptureV4L2.h"

namespace {
struct ModeDriver {
    bool interrupted = false;
    bool fail = false;
    bool ranges = false;
    bool invalid = false;
    std::vector<unsigned int> formats{V4L2_PIX_FMT_YUYV, V4L2_PIX_FMT_MJPEG, v4l2_fourcc('B','A','D','!')};
    int Query(unsigned long request, void* arg) {
        if (!interrupted) { interrupted = true; errno = EINTR; return -1; }
        if (fail) { errno = EIO; return -1; }
        if (request == VIDIOC_ENUM_FMT) {
            auto& f = *static_cast<v4l2_fmtdesc*>(arg);
            CHECK(f.type == V4L2_BUF_TYPE_VIDEO_CAPTURE);
            CHECK(f.reserved[0] == 0);
            if (f.index < formats.size()) { f.pixelformat = formats[f.index]; return 0; }
        } else if (request == VIDIOC_ENUM_FRAMESIZES) {
            auto& s = *static_cast<v4l2_frmsizeenum*>(arg);
            REQUIRE(s.pixel_format != v4l2_fourcc('B','A','D','!'));
            if (s.index == 0) {
                if (ranges) {
                    s.type = V4L2_FRMSIZE_TYPE_STEPWISE;
                    s.stepwise = {640, 1920, 16, 480, 1080, 4};
                } else {
                    s.type = V4L2_FRMSIZE_TYPE_DISCRETE;
                    s.discrete = {1920, 1080};
                }
                return 0;
            }
        } else if (request == VIDIOC_ENUM_FRAMEINTERVALS) {
            auto& i = *static_cast<v4l2_frmivalenum*>(arg);
            CHECK((i.width == 1920 || (ranges && i.width == 640)));
            CHECK((i.height == 1080 || (ranges && i.height == 480)));
            if (ranges && i.index == 0) {
                i.type = V4L2_FRMIVAL_TYPE_CONTINUOUS;
                i.stepwise.min = {1, 30};
                i.stepwise.max = {1, 5};
                return 0;
            }
            if (!ranges) {
                i.type = V4L2_FRMIVAL_TYPE_DISCRETE;
                if (i.pixel_format == V4L2_PIX_FMT_YUYV && i.index == 0) {
                    i.discrete = {1, 5}; return 0;
                }
                if (i.pixel_format == V4L2_PIX_FMT_MJPEG && i.index < 4) {
                    const v4l2_fract intervals[] = {{1,30}, {1001,30000}, {2,15}, {2,60}};
                    i.discrete = invalid ? v4l2_fract{0, 30} : intervals[i.index];
                    return 0;
                }
            }
        } else {
            FAIL("Discovery must only enumerate formats, sizes, and intervals");
        }
        errno = EINVAL;
        return -1;
    }
    std::vector<CameraCaptureMode> Modes() {
        return openshot::detail::EnumerateCameraModes([&](unsigned long r, void* v) { return Query(r, v); });
    }
};
}

TEST_CASE("V4L2 modes preserve format and exact frame rates", "[libopenshot][cameracapturereader][modes]")
{
    ModeDriver driver;
    const auto modes = driver.Modes();
    REQUIRE(modes.size() == 4); // Unknown FOURCC skipped, equivalent 30 fps deduplicated.
    CHECK(driver.interrupted); // Initial EINTR is retried.
    CHECK(modes[0].input_format == "yuyv422");
    CHECK(modes[0].fps.num == 5);
    CHECK(modes[1].input_format == "mjpeg");
    CHECK(modes[1].fps.num == 30);
    CHECK(modes[1].width == 1920);
    CHECK(modes[1].height == 1080);
    CHECK(modes[2].fps.num == 30000);
    CHECK(modes[2].fps.den == 1001);
    CHECK(modes[3].fps.num == 15);
    CHECK(modes[3].fps.den == 2);
}

TEST_CASE("V4L2 mode discovery handles unavailable and invalid data", "[libopenshot][cameracapturereader][modes]")
{
    ModeDriver driver;
    SECTION("No formats") { driver.formats.clear(); CHECK(driver.Modes().empty()); }
    SECTION("Invalid intervals are not invented as 30 fps") {
        driver.invalid = true;
        const auto modes = driver.Modes();
        REQUIRE(modes.size() == 1);
        CHECK(modes[0].fps.num == 5);
    }
    SECTION("Device failure is reported") { driver.fail = true; CHECK_THROWS_AS(driver.Modes(), std::runtime_error); }
    SECTION("Unsupported ioctl") {
        CHECK(openshot::detail::EnumerateCameraModes([](unsigned long, void*) { errno = ENOTTY; return -1; }).empty());
    }
    SECTION("Nonexistent device") {
        CHECK_THROWS_AS(CameraCaptureReader::GetDeviceModes("/dev/null/openshot-camera"), InvalidFile);
    }
    SECTION("Non-camera device") { CHECK(CameraCaptureReader::GetDeviceModes("/dev/null").empty()); }
}

TEST_CASE("V4L2 range endpoints remain valid mode combinations", "[libopenshot][cameracapturereader][modes]")
{
    ModeDriver driver;
    driver.ranges = true;
    const auto modes = driver.Modes();
    REQUIRE(modes.size() == 8);
    CHECK(modes[0].width == 640);
    CHECK(modes[0].height == 480);
    CHECK(modes[0].fps.num == 30);
    CHECK(modes[1].fps.num == 5);
    CHECK(modes[2].width == 1920);
    CHECK(modes[2].height == 1080);
    CHECK(modes[2].fps.num == 30);
}
#endif

TEST_CASE("Camera mode discovery leaves other backends untouched", "[libopenshot][cameracapturereader][modes]")
{
    CHECK(CameraCaptureReader::GetDeviceModes("unused", CAMERA_CAPTURE_WINDOWS_DSHOW).empty());
    CHECK(CameraCaptureReader::GetDeviceModes("unused", CAMERA_CAPTURE_MAC_AVFOUNDATION).empty());
}

#if defined(__linux__)
// Opt in with OPENSHOT_TEST_CAMERA=/dev/video0. Requires a camera advertising
// 1080p30 MJPEG, adequate lighting, and exclusive access for streaming.
TEST_CASE("V4L2 hardware captures advertised 1080p30 MJPEG", "[libopenshot][cameracapturereader][hardware]")
{
    const char* device = std::getenv("OPENSHOT_TEST_CAMERA");
    if (!device || !*device) {
        SUCCEED("Set OPENSHOT_TEST_CAMERA to enable the live camera test");
        return;
    }
    const auto modes = CameraCaptureReader::GetDeviceModes(device);
    bool supported = false;
    for (const auto& mode : modes)
        if (mode.width == 1920 && mode.height == 1080 && mode.input_format == "mjpeg" &&
            mode.fps.num == 30 && mode.fps.den == 1) supported = true;
    REQUIRE(supported);
    CameraCaptureSettings settings;
    settings.device = device;
    settings.width = 1920;
    settings.height = 1080;
    settings.fps = Fraction(30, 1);
    settings.options["input_format"] = "mjpeg";
    CameraCaptureReader reader(settings);
    reader.Open();
    REQUIRE(reader.info.width == 1920);
    REQUIRE(reader.info.height == 1080);
    REQUIRE(reader.info.fps.num == 30);
    REQUIRE(reader.info.fps.den == 1);
    // Discovery while streaming must not change the camera configuration.
    CHECK(CameraCaptureReader::GetDeviceModes(device).size() == modes.size());
    double first = 0, previous = 0;
    for (int i = 1; i <= 60; ++i) {
        const auto frame = reader.GetFrame(i);
        REQUIRE(frame != nullptr);
        if (i == 1) first = frame->capture_timestamp;
        else REQUIRE(frame->capture_timestamp > previous);
        previous = frame->capture_timestamp;
    }
    const double measured_fps = 59.0 / (previous - first);
    INFO("Measured source fps: " << measured_fps);
    CHECK(measured_fps > 27.0);
    CHECK(measured_fps < 33.0);
    reader.Close();
    CHECK_FALSE(reader.IsOpen());
}
#endif
