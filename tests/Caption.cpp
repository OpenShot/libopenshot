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
