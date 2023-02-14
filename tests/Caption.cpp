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
#include "openshot_catch.h"
#include <QApplication>
#include <QFontDatabase>
#include "effects/Caption.h"
#include "Clip.h"
#include "Frame.h"
#include "Timeline.h"


TEST_CASE( "caption effect", "[libopenshot][caption]" )
{
    // Check for QT Platform Environment variable - and ignore these tests if it's set to offscreen
    if (std::getenv("QT_QPA_PLATFORM") != nullptr) {
        std::string qt_platform_env = std::getenv("QT_QPA_PLATFORM");
        if (qt_platform_env == "offscreen") {
            std::cout << "Ignoring Caption unit tests due to invalid QT Platform: offscreen" << std::endl;
            return;
        }
    }

    int argc;
    char* argv[2];
    QGuiApplication::setAttribute(Qt::AA_EnableHighDpiScaling);
    QApplication app(argc, argv);
    QApplication::processEvents();

    int check_row = 0;
    int check_col = 0;

    SECTION("default constructor") {

        // Create an empty caption
        openshot::Caption c1;

        CHECK(c1.color.GetColorHex(1) == "#ffffff");
        CHECK(c1.stroke.GetColorHex(1) == "#a9a9a9");
        CHECK(c1.background.GetColorHex(1) == "#000000");
        CHECK(c1.background_alpha.GetValue(1) == Approx(0.0f).margin(0.00001));
        CHECK(c1.left.GetValue(1) == Approx(0.15f).margin(0.00001));
        CHECK(c1.right.GetValue(1) == Approx(0.15f).margin(0.00001));
        CHECK(c1.top.GetValue(1) == Approx(0.7).margin(0.00001));
        CHECK(c1.stroke_width.GetValue(1) == Approx(0.5f).margin(0.00001));
        CHECK(c1.font_size.GetValue(1) == Approx(30.0f).margin(0.00001));
        CHECK(c1.font_alpha.GetValue(1) == Approx(1.0f).margin(0.00001));
        CHECK(c1.font_name == "sans");
        CHECK(c1.fade_in.GetValue(1) == Approx(0.35f).margin(0.00001));
        CHECK(c1.fade_out.GetValue(1) == Approx(0.35f).margin(0.00001));
        CHECK(c1.background_corner.GetValue(1) == Approx(10.0f).margin(0.00001));
        CHECK(c1.background_padding.GetValue(1) == Approx(20.0f).margin(0.00001));
        CHECK(c1.line_spacing.GetValue(1) == Approx(1.0f).margin(0.00001));
        CHECK(c1.CaptionText() == "00:00:00:000 --> 00:10:00:000\nEdit this caption with our caption editor");

        // Load clip with video
        std::stringstream path;
        path << TEST_MEDIA_PATH << "sintel_trailer-720p.mp4";
        openshot::Clip clip1(path.str());
        clip1.Open();

        // Add Caption effect
        clip1.AddEffect(&c1);

        // Get frame
        std::shared_ptr<openshot::Frame> f = clip1.GetFrame(10);

#ifdef _WIN32
        // Windows pixel location
        check_col = 300;
        check_row = 524;
#else
        // Linux/Mac pixel location
        check_col = 252;
        check_row = 523;
#endif

        // Verify pixel values (black background pixels)
        const unsigned char *pixels = f->GetPixels(1);
        CHECK((int) pixels[0 * 4] == 0);

        // Verify pixel values (white text pixels)
        pixels = f->GetPixels(check_row);
        CHECK((int) pixels[check_col * 4] == 255);

        // Create Timeline
        openshot::Timeline t(1280, 720, openshot::Fraction(24, 1), 44100, 2, openshot::LAYOUT_STEREO);
        t.AddClip(&clip1);

        // Get timeline frame
        f = t.GetFrame(10);

#ifdef _WIN32
        // Windows pixel location
        check_col = 300;
        check_row = 523;
#else
        // Linux/Mac pixel location
        check_col = 252;
        check_row = 523;
#endif

        // Verify pixel values (black background pixels)
        pixels = f->GetPixels(1);
        CHECK((int) pixels[0 * 4] == 0);

        // Verify pixel values (white text pixels)
        pixels = f->GetPixels(check_row);
        CHECK((int) pixels[check_col * 4] == 255);

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

#ifdef _WIN32
        // Windows pixel location
        check_col = 146;
        check_row = 355;
#else
        // Linux/Mac pixel location
        check_col = 117;
        check_row = 355;
#endif

        // Verify pixel values (black background pixels)
        const unsigned char *pixels = f->GetPixels(1);
        CHECK((int) pixels[0 * 4] == 0);

        // Verify pixel values (white text pixels)
        pixels = f->GetPixels(check_row);
        CHECK((int) pixels[check_col * 4] == 255);

        // Create Timeline
        openshot::Timeline t(720, 480, openshot::Fraction(24, 1), 44100, 2, openshot::LAYOUT_STEREO);
        t.AddClip(&clip1);

        // Get timeline frame
        f = t.GetFrame(10);

#ifdef _WIN32
        // Windows pixel location
        check_col = 146;
        check_row = 355;
#else
        // Linux/Mac pixel location
        check_col = 117;
        check_row = 355;
#endif

        // Verify pixel values (black background pixels)
        pixels = f->GetPixels(1);
        CHECK((int) pixels[0 * 4] == 0);

        // Verify pixel values (white text pixels)
        pixels = f->GetPixels(check_row);
        CHECK((int) pixels[check_col * 4] == 255);

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
        std::shared_ptr<openshot::Frame> f = clip1.GetFrame(10);

#ifdef _WIN32
        // Windows pixel location
        check_col = 325;
        check_row = 522;
#else
        // Linux/Mac pixel location
        check_col = 318;
        check_row = 521;
#endif

        // Verify pixel values (black background pixels)
        const unsigned char *pixels = f->GetPixels(1);
        CHECK((int) pixels[0 * 4] == 0);

        // Verify pixel values (white text pixels)
        pixels = f->GetPixels(check_row);
        CHECK((int) pixels[check_col * 4] == 255);

        // Close objects
        clip1.Close();
    }

    // Close QApplication
    app.quit();
}