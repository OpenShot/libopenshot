/**
 * @file
 * @brief Unit tests for openshot::Caption
 * @author Jonathan Thomas <jonathan@openshot.org>
 *
 * @ref License
 */

// Copyright (c) 2008-2023 OpenShot Studios, LLC
//
// SPDX-License-Identifier: LGPL-3.0-or-later


#include <vector>
#include <sstream>
#include "openshot_catch.h"
#include <QApplication>
#include <QFontDatabase>
#include <QPainter>
#include <algorithm>
#include "effects/Caption.h"
#include "Clip.h"
#include "Frame.h"
#include "Timeline.h"


// Function to check for non-black pixels in a given region of the frame
bool HasNonBlackPixelsInRegion(const std::shared_ptr<openshot::Frame>& frame, int start_row, int end_row, int start_col, int end_col) {
    int frame_width = frame->GetWidth();
    int frame_height = frame->GetHeight();

    // Ensure the search region is within the frame bounds
    if (start_row < 0 || end_row >= frame_height || start_col < 0 || end_col >= frame_width) {
        throw std::out_of_range("Search region is out of frame bounds");
    }

    for (int row = start_row; row <= end_row; ++row) {
        const unsigned char* pixels = frame->GetPixels(row);
        if (!pixels) {
            throw std::runtime_error("Failed to get pixels for the row");
        }

        for (int col = start_col; col <= end_col; ++col) {
            int index = col * 4;
            int R = pixels[index];
            int G = pixels[index + 1];
            int B = pixels[index + 2];
            if (!(R == 0 && G == 0 && B == 0)) {
                return true;  // Non-black pixel found
            }
        }
    }
    return false;  // No non-black pixel found
}

TEST_CASE("caption effect", "[libopenshot][caption]") {
    // Check for QT Platform Environment variable - and ignore these tests if it's set to offscreen
    if (std::getenv("QT_QPA_PLATFORM") != nullptr) {
        std::string qt_platform_env = std::getenv("QT_QPA_PLATFORM");
        if (qt_platform_env == "offscreen") {
            std::cout << "Ignoring Caption unit tests due to invalid QT Platform: offscreen" << std::endl;
            return;
        }
    }

    int argc = 1;
    char* argv[1] = {(char*)""};
#if QT_VERSION < QT_VERSION_CHECK(6, 0, 0)
    QGuiApplication::setAttribute(Qt::AA_EnableHighDpiScaling);
#endif
    QApplication app(argc, argv);

    QApplication::processEvents();

    SECTION("default constructor") {

        // Create an empty caption
        openshot::Caption c1;

        CHECK(c1.color.GetColorHex(1) == "#ffffff");
        CHECK(c1.stroke.GetColorHex(1) == "#a9a9a9");
        CHECK(c1.background.GetColorHex(1) == "#000000");
        CHECK(c1.background_alpha.GetValue(1) == Approx(0.0f).margin(0.00001));
        CHECK(c1.left.GetValue(1) == Approx(0.10f).margin(0.00001));
        CHECK(c1.right.GetValue(1) == Approx(0.10f).margin(0.00001));
        CHECK(c1.top.GetValue(1) == Approx(0.75).margin(0.00001));
        CHECK(c1.bottom.GetValue(1) == Approx(0.0).margin(0.00001));
        CHECK(c1.stroke_width.GetValue(1) == Approx(0.5f).margin(0.00001));
        CHECK(c1.font_size.GetValue(1) == Approx(30.0f).margin(0.00001));
        CHECK(c1.font_alpha.GetValue(1) == Approx(1.0f).margin(0.00001));
        CHECK(c1.font_name == "sans");
        CHECK(c1.fade_in.GetValue(1) == Approx(0.0f).margin(0.00001));
        CHECK(c1.fade_out.GetValue(1) == Approx(0.0f).margin(0.00001));
        CHECK(c1.background_corner.GetValue(1) == Approx(10.0f).margin(0.00001));
        CHECK(c1.background_padding.GetValue(1) == Approx(20.0f).margin(0.00001));
        CHECK(c1.line_spacing.GetValue(1) == Approx(1.0f).margin(0.00001));
        CHECK(c1.CaptionText() == "00:00:00:000 --> 00:10:00:000\nEdit this caption with our caption editor");

        Json::Value caption_json = c1.JsonValue();
        REQUIRE(caption_json["bottom"].isObject());
        caption_json["bottom"] = openshot::Keyframe(0.25).JsonValue();
        openshot::Caption c2;
        c2.SetJsonValue(caption_json);
        CHECK(c2.bottom.GetValue(1) == Approx(0.25).margin(0.00001));

        Json::Value properties = openshot::stringToJson(c1.PropertiesJSON(1));
        CHECK(properties["top"]["name"].asString() == "Margin: Top");
        CHECK(properties["left"]["name"].asString() == "Margin: Left");
        CHECK(properties["bottom"]["name"].asString() == "Margin: Bottom");
        CHECK(properties["right"]["name"].asString() == "Margin: Right");

        // Load clip with video
        std::stringstream path;
        path << TEST_MEDIA_PATH << "sintel_trailer-720p.mp4";
        openshot::Clip clip1(path.str());
        clip1.Open();

        // Add Caption effect
        clip1.AddEffect(&c1);

        // Get frame
        std::shared_ptr<openshot::Frame> f = clip1.GetFrame(10);

        // Verify pixel values (black background pixels)
        const unsigned char* pixels = f->GetPixels(1);
        CHECK((int)pixels[0 * 4] == 0);

        // Check for non-black pixels in the region for white text
        CHECK(HasNonBlackPixelsInRegion(f, 560, 700, 200, 600));

        // Create Timeline
        openshot::Timeline t(1280, 720, openshot::Fraction(24, 1), 44100, 2, openshot::LAYOUT_STEREO);
        t.AddClip(&clip1);

        // Get timeline frame
        f = t.GetFrame(10);

        // Verify pixel values (black background pixels)
        pixels = f->GetPixels(1);
        CHECK((int)pixels[0 * 4] == 0);

        // Check for non-black pixels in the region for white text
        CHECK(HasNonBlackPixelsInRegion(f, 560, 700, 200, 600));

        // Close objects
        t.Close();
        clip1.Close();
    }

    SECTION("audio captions") {
        // Create an empty caption
        openshot::Caption c1;

        // Load clip with audio file
        std::stringstream path;
        path << TEST_MEDIA_PATH << "piano.wav";
        openshot::Clip clip1(path.str());
        clip1.Open();

        // Add Caption effect
        clip1.AddEffect(&c1);

        // Get frame
        std::shared_ptr<openshot::Frame> f = clip1.GetFrame(10);
        f->Save("/home/jonathan/test.png", 1.0, "PNG", 100);

        // Verify pixel values (black background pixels)
        const unsigned char* pixels = f->GetPixels(1);
        CHECK((int)pixels[0 * 4] == 0);

        // Check for non-black pixels in the region for white text
        CHECK(HasNonBlackPixelsInRegion(f, 350, 479, 150, 500));

        // Create Timeline
        openshot::Timeline t(720, 480, openshot::Fraction(24, 1), 44100, 2, openshot::LAYOUT_STEREO);
        t.AddClip(&clip1);

        // Get timeline frame
        f = t.GetFrame(10);

        // Verify pixel values (black background pixels)
        pixels = f->GetPixels(1);
        CHECK((int)pixels[0 * 4] == 0);

        // Check for non-black pixels in the region for white text
        CHECK(HasNonBlackPixelsInRegion(f, 200, 479, 200, 600));

        // Close objects
        t.Close();
        clip1.Close();
    }

    SECTION("long single-line caption") {
        // Create an single-line long caption
        std::string caption_text = "00:00.000 --> 00:10.000\nそれが今のF1レースでは時速300kmですから、すごい進歩です。命知らずのレーザーたちによって車のスピードは更新されていったのです。";
        openshot::Caption c1(caption_text);

        // Load clip with video file
        std::stringstream path;
        path << TEST_MEDIA_PATH << "sintel_trailer-720p.mp4";
        openshot::Clip clip1(path.str());
        clip1.Open();

        // Add Caption effect
        clip1.AddEffect(&c1);

        // Get frame
        std::shared_ptr<openshot::Frame> f = clip1.GetFrame(11);

        // Verify pixel values (black background pixels)
        const unsigned char *pixels = f->GetPixels(1);
        CHECK((int) pixels[0 * 4] == 0);

        // Check for non-black pixels in the region for white text
        CHECK(HasNonBlackPixelsInRegion(f, 560, 700, 200, 600));

        // Close objects
        clip1.Close();
    }


    SECTION("stroke before fill regression") {
        // Compare the filled interior of caption glyphs with and
        // without a thick outline. The outline must not darken
        // pixels that belong to the original glyph fill.
        const std::string text =
            "00:00.000 --> 00:10.000\nOPEN";

        openshot::Caption fill_only(text);
        fill_only.color = openshot::Color("#ffffff");
        fill_only.stroke = openshot::Color("#000000");
        fill_only.stroke_width = openshot::Keyframe(0.0);
        fill_only.font_size = openshot::Keyframe(110.0);
        fill_only.top = openshot::Keyframe(0.2);

        openshot::Caption outlined(text);
        outlined.color = openshot::Color("#ffffff");
        outlined.stroke = openshot::Color("#000000");
        outlined.stroke_width = openshot::Keyframe(16.0);
        outlined.font_size = openshot::Keyframe(110.0);
        outlined.top = openshot::Keyframe(0.2);

        // The timeline supplies the FPS used by caption cue timing.
        openshot::Timeline timeline(
            600, 300, openshot::Fraction(24, 1),
            44100, 2, openshot::LAYOUT_STEREO);

        timeline.AddEffect(&fill_only);

        auto base_frame = std::make_shared<openshot::Frame>(
            1, 600, 300, "#000000");
        auto base = fill_only.GetFrame(base_frame, 1);

        timeline.RemoveEffect(&fill_only);
        timeline.AddEffect(&outlined);

        auto outlined_frame = std::make_shared<openshot::Frame>(
            1, 600, 300, "#000000");
        auto result = outlined.GetFrame(outlined_frame, 1);

        int fill_pixels = 0;
        int darkened_pixels = 0;

        for (int y = 0; y < 300; ++y) {
            const unsigned char* baseline = base->GetPixels(y);
            const unsigned char* actual = result->GetPixels(y);

            for (int x = 0; x < 600; ++x) {
                const int index = x * 4;

                if (baseline[index] < 240 ||
                    baseline[index + 1] < 240 ||
                    baseline[index + 2] < 240)
                    continue;

                ++fill_pixels;

                if (actual[index] < baseline[index] - 10)
                    ++darkened_pixels;
            }
        }

        INFO("Fill pixels: " << fill_pixels);
        INFO("Darkened fill pixels: " << darkened_pixels);

        REQUIRE(fill_pixels > 0);
        CHECK(darkened_pixels == 0);

        timeline.RemoveEffect(&outlined);
        timeline.Close();
    }

    // Close QApplication
    app.quit();
}

TEST_CASE("Caption fades composite the complete overlay on the CPU", "[caption]") {
    int argc = 1;
    char name[] = "caption-fade-test";
    char* argv[] = {name, nullptr};
    QApplication app(argc, argv);
    openshot::Timeline timeline(600, 300, openshot::Fraction(24, 1), 44100, 2, openshot::LAYOUT_STEREO);
    openshot::Caption caption("00:00.000 --> 00:02.000\nComplete caption");
    timeline.AddEffect(&caption);
    caption.top = openshot::Keyframe(0.2);
    caption.background_alpha = openshot::Keyframe(0.8);
    caption.background = openshot::Color("#204080");
    caption.stroke_width = openshot::Keyframe(4.0);
    auto full = std::make_shared<openshot::Frame>(13, 600, 300, "#00000000");
    auto transparent = std::make_shared<QImage>(600, 300, QImage::Format_RGBA8888_Premultiplied);
    transparent->fill(Qt::transparent);
    full->AddImage(transparent);
    caption.GetFrame(full, 13);
    const QImage overlay = full->GetImage()->copy();
    caption.fade_in = openshot::Keyframe(1.0);
    caption.fade_out = openshot::Keyframe(1.0);
    const std::vector<std::pair<int, double>> samples = {
        {1, 0.0}, {7, 0.25}, {13, 0.5}, {19, 0.75}, {25, 23.0 / 24.0},
        {36, 0.5}, {48, 0.0}, {49, 0.0}, {13, 0.5}};
    for (const auto& sample : samples) {
        INFO("Frame " << sample.first);
        QImage expected(600, 300, QImage::Format_RGBA8888_Premultiplied);
        expected.fill(QColor(90, 30, 60, 210));
        auto frame = std::make_shared<openshot::Frame>(sample.first, 600, 300, "#00000000");
        frame->AddImage(std::make_shared<QImage>(expected));
        QPainter painter(&expected);
        painter.setOpacity(sample.second);
        painter.drawImage(0, 0, overlay);
        painter.end();
        caption.GetFrame(frame, sample.first);
        const auto actual = frame->GetImage()->convertToFormat(QImage::Format_RGBA8888_Premultiplied);
        int largest_error = 0;
        for (int y = 0; y < actual.height(); ++y)
            for (int x = 0; x < actual.width() * 4; ++x)
                largest_error = std::max(largest_error, std::abs(int(actual.constScanLine(y)[x]) - int(expected.constScanLine(y)[x])));
        CHECK(largest_error <= 2);
    }
    timeline.RemoveEffect(&caption);
}

#ifdef OPENSHOT_ENABLE_GPU_FRAME
#include "VulkanFrameImage.h"
#include "GpuMetrics.h"
#include "GpuDevices.h"
#include "FFmpegReader.h"
#include "FFmpegWriter.h"
#include "QtImageReader.h"
#include <QTemporaryDir>
#include <algorithm>

namespace {
int CaptionPixelError(const std::shared_ptr<openshot::Frame>& cpu,
                      const std::shared_ptr<openshot::Frame>& gpu) {
    const auto expected = cpu->GetImage()->convertToFormat(QImage::Format_RGBA8888_Premultiplied);
    const auto actual = gpu->GetImage()->convertToFormat(QImage::Format_RGBA8888_Premultiplied);
    int largest_error = 0;
    for (int y = 0; y < actual.height(); ++y)
        for (int x = 0; x < actual.width() * 4; ++x)
            largest_error = std::max(largest_error, std::abs(int(actual.constScanLine(y)[x]) - int(expected.constScanLine(y)[x])));
    return largest_error;
}
}

TEST_CASE("Vulkan captions preserve video surfaces and match CPU rendering", "[caption][vulkan]") {
    if (!openshot::VulkanRenderingEnabled() || openshot::ActiveGpuDevice().name.empty()) {
        WARN("Skipping Vulkan test: no Vulkan device is available");
        return;
    }
    int argc = 1;
    char name[] = "caption-test";
    char* argv[] = {name, nullptr};
    QApplication app(argc, argv);
    openshot::Timeline timeline(600, 300, openshot::Fraction(24, 1), 44100, 2, openshot::LAYOUT_STEREO);
    openshot::Caption caption("00:01.000 --> 00:03.000\nFirst caption\nSecond line\n\n"
                              "00:03.500 --> 00:04.000\nNext cue");
    timeline.AddEffect(&caption);
    caption.top = openshot::Keyframe(0.2);
    caption.bottom = openshot::Keyframe(0.15);
    caption.background_alpha = openshot::Keyframe(0.6);
    caption.font_alpha = openshot::Keyframe(0.7);
    caption.stroke_width = openshot::Keyframe(2.0);
    caption.fade_in = openshot::Keyframe(0.5);
    caption.fade_out = openshot::Keyframe(0.5);
    caption.font_size.AddPoint(40, 30.0, openshot::LINEAR);
    caption.font_size.AddPoint(60, 45.0, openshot::LINEAR);

    // A partially transparent source also checks premultiplied source-over.
    QImage input(600, 300, QImage::Format_RGBA8888_Premultiplied);
    input.fill(QColor(25, 65, 110, 180));
    int iteration = 0;
    for (int n : {1, 25, 30, 40, 40, 41, 40, 65, 72, 73, 85, 90, 96, 97}) {
        // Change the video under repeated cues to detect accidental video caching.
        input.fill(iteration++ % 2 ? QColor(25, 65, 110, 180) : QColor(90, 30, 60, 210));
        auto surface = openshot::UploadVulkanImage(input);
        INFO("Frame " << n);
        auto cpu = std::make_shared<openshot::Frame>(n, 600, 300, "#00000000");
        cpu->AddImage(std::make_shared<QImage>(input));
        caption.GetFrame(cpu, n);
        auto gpu = std::make_shared<openshot::Frame>(n, 600, 300, "#00000000");
        gpu->AddDeferredImage(surface);
        const auto before = openshot::VulkanFrameReadbackCount();
        caption.ProcessFrame(gpu, n);
        REQUIRE(gpu->GetDeferredImage());
        CHECK(openshot::VulkanFrameReadbackCount() == before);
        if (n == 1 || n == 73 || n == 97)
            CHECK(gpu->GetDeferredImage() == surface);
        CHECK(CaptionPixelError(cpu, gpu) <= 3);
    }

    // Editing text/style and changing resolution must invalidate a static cue.
    caption.fade_in = openshot::Keyframe(0.0);
    caption.fade_out = openshot::Keyframe(0.0);
    QTemporaryDir directory;
    REQUIRE(directory.isValid());
    QImage mask(600, 300, QImage::Format_RGB888);
    mask.fill(QColor(128, 128, 128));
    const auto mask_path = directory.filePath("mask.png").toStdString();
    REQUIRE(mask.save(QString::fromStdString(mask_path)));
    for (int pass = 0; pass < 6; ++pass) {
        INFO("Edit pass " << pass);
        if (pass == 1) caption.CaptionText("00:00.000 --> 00:10.000\nChanged caption");
        if (pass == 2) caption.color = openshot::Color("#ff0000");
        if (pass == 4) caption.MaskReader(new openshot::QtImageReader(mask_path));
        if (pass == 5) caption.mask_invert = true;
        const int width = pass == 3 ? 300 : 600;
        QImage pixels(width, 300, QImage::Format_RGBA8888_Premultiplied);
        pixels.fill(Qt::black);
        auto cpu = std::make_shared<openshot::Frame>(40, width, 300, "#000000");
        cpu->AddImage(std::make_shared<QImage>(pixels));
        caption.ProcessFrame(cpu, 40);
        auto gpu = std::make_shared<openshot::Frame>(40, width, 300, "#000000");
        gpu->AddDeferredImage(openshot::UploadVulkanImage(pixels));
        const auto before = openshot::VulkanFrameReadbackCount();
        caption.ProcessFrame(gpu, 40);
        REQUIRE(gpu->GetDeferredImage());
        CHECK(openshot::VulkanFrameReadbackCount() == before);
        CHECK(CaptionPixelError(cpu, gpu) <= 3);
    }
    timeline.RemoveEffect(&caption);
}

TEST_CASE("Vulkan Caption fades reuse one uploaded overlay", "[caption][vulkan]") {
    if (!openshot::VulkanRenderingEnabled() || openshot::ActiveGpuDevice().name.empty()) {
        WARN("Skipping Vulkan test: no Vulkan device is available");
        return;
    }
    int argc = 1;
    char name[] = "caption-upload-test";
    char* argv[] = {name, nullptr};
    QApplication app(argc, argv);
    openshot::Timeline timeline(600, 300, openshot::Fraction(24, 1), 44100, 2, openshot::LAYOUT_STEREO);
    openshot::Caption caption("00:00.000 --> 00:02.000\nOne uploaded caption");
    timeline.AddEffect(&caption);
    caption.top = openshot::Keyframe(0.2);
    caption.background_alpha = openshot::Keyframe(0.8);
    caption.fade_in = openshot::Keyframe(0.5);
    caption.fade_out = openshot::Keyframe(0.5);
    QImage input(600, 300, QImage::Format_RGBA8888_Premultiplied);
    input.fill(Qt::black);
    auto surface = openshot::UploadVulkanImage(input);
    openshot::ResetGpuMetrics();
    const auto readbacks = openshot::VulkanFrameReadbackCount();
    for (int n = 1; n <= 49; ++n) {
        openshot::gpu_metrics::Scope scope(false);
        auto frame = std::make_shared<openshot::Frame>(n, 600, 300, "#000000");
        frame->AddDeferredImage(surface);
        caption.GetFrame(frame, n);
        REQUIRE(frame->GetDeferredImage());
    }
    auto counts = openshot::stringToJson(openshot::GpuMetricsSnapshot());
    CHECK(counts["uploads"].asUInt64() == 1);
    CHECK(openshot::VulkanFrameReadbackCount() == readbacks);
    // A style edit must still produce a new raster/upload.
    caption.color = openshot::Color("#ff0000");
    {
        openshot::gpu_metrics::Scope scope(false);
        auto frame = std::make_shared<openshot::Frame>(13, 600, 300, "#000000");
        frame->AddDeferredImage(surface);
        caption.GetFrame(frame, 13);
    }
    counts = openshot::stringToJson(openshot::GpuMetricsSnapshot());
    CHECK(counts["uploads"].asUInt64() == 2);
    CHECK(openshot::VulkanFrameReadbackCount() == readbacks);
    timeline.RemoveEffect(&caption);
}

TEST_CASE("Vulkan H264 decode caption encode requires no video readbacks", "[caption][vulkan][hardware][writer]") {
    if (!openshot::VulkanRenderingEnabled() || openshot::ActiveGpuDevice().name.empty()) {
        WARN("Skipping Vulkan test: no Vulkan device is available");
        return;
    }
    const auto encoders = openshot::GpuEncoders();
    const auto encoder = std::find(encoders.begin(), encoders.end(), "h264_vulkan") != encoders.end()
        ? "h264_vulkan" : "h264_nvenc";
    if (std::find(encoders.begin(), encoders.end(), encoder) == encoders.end()) {
        WARN("Skipping Vulkan test: no GPU H.264 encoder");
        return;
    }
    int argc = 1;
    char name[] = "caption-export-test";
    char* argv[] = {name, nullptr};
    QApplication app(argc, argv);
    QTemporaryDir directory;
    REQUIRE(directory.isValid());
    openshot::FFmpegReader reader(TEST_MEDIA_PATH "sintel_trailer-720p.mp4");
    openshot::Caption caption("00:00.000 --> 00:00.500\nVulkan caption\nSecond line");
    caption.top = openshot::Keyframe(0.25);
    caption.background_alpha = openshot::Keyframe(0.7);
    caption.fade_in = openshot::Keyframe(0.2);
    caption.fade_out = openshot::Keyframe(0.2);
    openshot::Clip clip(&reader);
    clip.AddEffect(&caption);
    openshot::Timeline timeline(640, 360, openshot::Fraction(24, 1), 48000, 2, openshot::LAYOUT_STEREO);
    timeline.SetMaxSize(640, 360);
    timeline.AddClip(&clip);
    timeline.Open();
    const auto path = directory.filePath("caption.mp4").toStdString();
    openshot::FFmpegWriter writer(path);
    writer.SetVideoOptions(true, encoder, openshot::Fraction(24, 1), 640, 360,
                           openshot::Fraction(1, 1), false, false, 2000000);
    writer.Open();
    REQUIRE(writer.info.vcodec == encoder);
    std::shared_ptr<openshot::Frame> reference;
    const auto before = openshot::VulkanFrameReadbackCount();
    for (int n = 1; n <= 24; ++n) {
        auto frame = timeline.GetFrame(n);
        REQUIRE(frame->GetDeferredImage());
        writer.WriteFrame(frame);
        REQUIRE(frame->GetDeferredImage());
        if (n == 8) reference = frame;
    }
    writer.Close();
    CHECK(reader.HardwareDecodeSuccessful());
    CHECK(openshot::VulkanFrameReadbackCount() == before);
    timeline.Close();
    openshot::FFmpegReader encoded(path);
    encoded.Open();
    CHECK(encoded.info.video_length == 24);
    CHECK(encoded.info.width == 640);
    CHECK(encoded.info.height == 360);
    const auto expected = reference->GetImage()->convertToFormat(QImage::Format_RGB888);
    const auto actual = encoded.GetFrame(8)->GetImage()->convertToFormat(QImage::Format_RGB888);
    double error = 0.0;
    for (int y = 0; y < actual.height(); ++y)
        for (int x = 0; x < actual.width() * 3; ++x)
            error += std::abs(int(actual.constScanLine(y)[x]) - int(expected.constScanLine(y)[x]));
    CHECK(error / (actual.width() * actual.height() * 3) < 10.0);
    encoded.Close();
    clip.RemoveEffect(&caption);
}
#endif
