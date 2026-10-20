/**
 * @file
 * @brief Unit tests for openshot::QtTextReader
 * @author Jonathan Thomas <jonathan@openshot.org>
 *
 * @ref License
 */

// Copyright (c) 2008-2026 OpenShot Studios, LLC
//
// SPDX-License-Identifier: LGPL-3.0-or-later

#include "openshot_catch.h"
#include "QtTextReader.h"
#include "Clip.h"
#include "Timeline.h"
#include "Color.h"
#include "Frame.h"
#include <QImage>
#include <QFont>
#include <QGuiApplication>

using namespace openshot;

namespace {
int text_argc = 1;
char text_name[] = "text-test";
char* text_argv[] = {text_name, nullptr};

int& system_font_args()
{
#ifdef _WIN32
    // Offscreen reads fonts only from <Qt>/lib/fonts on Windows, which Qt no
    // longer ships; without fonts, text renders no pixels.
    if (!qEnvironmentVariableIsSet("QT_QPA_FONTDIR")) {
        QByteArray root = qgetenv("SystemRoot");
        qputenv("QT_QPA_FONTDIR", (root.isEmpty() ? QByteArray("C:\\Windows") : root) + "\\Fonts");
    }
#endif
    return text_argc;
}

struct TextTestApplication : QGuiApplication {
    TextTestApplication() : QGuiApplication(system_font_args(), text_argv) {}
};
}

TEST_CASE("Text source survives clip JSON round trip without a path", "[libopenshot][qttextreader]")
{
    TextTestApplication application;
    QtTextReader source;
    Json::Value data = source.JsonValue();
    data["width"] = 320;
    data["height"] = 180;
    data["text"] = "Hello\n世界";
    QFont font("DejaVu Sans");
    font.setPixelSize(32);
    data["font"] = font.toString().toStdString();
    data["text_color"] = "#ff0000";
    data["background_color"] = "transparent";
    data["gravity"] = GRAVITY_TOP_LEFT;
    data["document_layout"] = true;
    data["padding"] = 12;
    data["stroke_width"] = 1.0;
    data["stroke_color"] = "#000000";
    data["border_width"] = 0.0;
    data["border_color"] = "#ffffff";
    source.SetJsonValue(data);
    Clip clip;
    Json::Value json = clip.JsonValue();
    json["reader"] = source.JsonValue();
    clip.SetJsonValue(json);
    clip.Open();
    CHECK(clip.Reader()->Name() == "QtTextReader");
    auto image = clip.GetFrame(1)->GetImage();
    CHECK(image->width() == 320);
    CHECK(image->height() == 180);
    CHECK(image->pixelColor(0, 0).alpha() == 0);
    bool found_red = false;
    for (int y = 0; y < image->height(); ++y)
        for (int x = 0; x < image->width(); ++x) {
            auto color = image->pixelColor(x, y);
            found_red |= color.alpha() > 0 && color.red() > color.green();
        }
    CHECK(found_red);
    auto saved = clip.JsonValue();
    CHECK(saved["reader"]["text"].asString() == "Hello\n世界");
    CHECK(saved["reader"]["stroke_width"].asDouble() == 1.0);
    CHECK(saved["reader"]["document_layout"].asBool());
    clip.Close();
    Clip restored;
    restored.SetJsonValue(saved);
    restored.Open();
    CHECK(*restored.GetFrame(1)->GetImage() == *image);
    restored.Close();
}

TEST_CASE("Text reader re-renders edits and preserves legacy layout", "[libopenshot][qttextreader]")
{
    TextTestApplication application;
    QtTextReader reader;
    CHECK_FALSE(reader.JsonValue()["document_layout"].asBool());
    reader.Open();
    auto before = reader.GetFrame(1)->GetImage()->copy();
    Json::Value edit;
    edit["text"] = "Updated";
    edit["background_color"] = "#ff0000";
    reader.SetJsonValue(edit);
    CHECK(reader.GetFrame(1)->GetImage()->copy() != before);
    reader.Close();
}

TEST_CASE("Text style curves render through the timeline and survive JSON", "[libopenshot][qttextreader]")
{
    TextTestApplication application;
    QtTextReader reader;
    Json::Value data = reader.JsonValue();
    data["width"] = 320;
    data["height"] = 180;
    data["text"] = "Animated";
    QFont font("DejaVu Sans");
    font.setPixelSize(40);
    data["font"] = font.toString().toStdString();
    data["document_layout"] = true;
    data["background_color"] = "transparent";
    Color color("#ff0000");
    color.red.AddPoint(31, 0, LINEAR);
    color.blue.AddPoint(31, 255, LINEAR);
    data["text_animation"]["color"] = color.JsonValue();
    reader.SetJsonValue(data);
    reader.Open();
    CHECK_FALSE(reader.info.has_single_image);
    Clip clip(&reader);
    clip.End(2.0);
    Timeline timeline(320, 180, Fraction(30, 1), 44100, 2, LAYOUT_STEREO);
    timeline.AddClip(&clip);
    timeline.Open();
    auto first = timeline.GetFrame(1)->GetImage()->copy();
    QPoint sample(0, 0);
    int brightest = 0;
    for (int y = 0; y < first.height(); ++y)
        for (int x = 0; x < first.width(); ++x)
            if (first.pixelColor(x, y).red() > brightest) {
                brightest = first.pixelColor(x, y).red();
                sample = QPoint(x, y);
            }
    REQUIRE(brightest > 180);
    CHECK(first.pixelColor(sample).blue() == 0);
    auto middle = timeline.GetFrame(16)->GetImage()->pixelColor(sample);
    CHECK(std::abs(middle.red() - brightest / 2) <= 2);
    CHECK(std::abs(middle.blue() - brightest / 2) <= 2);
    auto last = timeline.GetFrame(31)->GetImage()->pixelColor(sample);
    CHECK(last.red() == 0);
    CHECK(std::abs(last.blue() - brightest) <= 2);
    CHECK(reader.JsonValue()["text_color"].asString() == "#ffffff");
    CHECK(reader.JsonValue()["text_animation"] == data["text_animation"]);
    timeline.Close();
    reader.Close();
}

TEST_CASE("Unscaled text keeps its project size in a smaller preview", "[libopenshot][qttextreader]")
{
    TextTestApplication application;
    QtTextReader reader;
    Json::Value data = reader.JsonValue();
    data["width"] = 640;
    data["height"] = 90;
    data["text"] = "Preview";
    QFont font("DejaVu Sans");
    font.setPixelSize(48);
    data["font"] = font.toString().toStdString();
    data["document_layout"] = true;
    data["background_color"] = "transparent";
    reader.SetJsonValue(data);
    const auto text_span = [](const QImage& image) {
        int left = image.width(), right = -1;
        for (int y = 0; y < image.height(); ++y)
            for (int x = 0; x < image.width(); ++x)
                if (qRed(image.pixel(x, y)) > 128) {
                    left = std::min(left, x);
                    right = std::max(right, x);
                }
        return right < left ? 0.0 : (right - left + 1) / double(image.width());
    };
    std::vector<double> spans;
    for (int preview_width : {1280, 320}) {
        Clip clip(&reader);
        clip.scale = SCALE_NONE;
        Timeline timeline(1280, 720, Fraction(30, 1), 44100, 2, LAYOUT_STEREO);
        timeline.AddClip(&clip);
        timeline.SetMaxSize(preview_width, preview_width * 9 / 16);
        timeline.Open();
        spans.push_back(text_span(*timeline.GetFrame(1)->GetImage()));
        timeline.Close();
    }
    REQUIRE(spans[0] > 0.05);
    // A 640px source drawn into a 320px preview must not stay 640px wide.
    CHECK(std::abs(spans[1] - spans[0]) < 0.02);
    reader.Close();
}

namespace {
// A solid background animated from red at frame 1 to blue at frame 61.
Json::Value animated_background(QtTextReader& reader, const Fraction& fps)
{
    Json::Value data = reader.JsonValue();
    data["width"] = 160;
    data["height"] = 90;
    data["text"] = "";
    data["document_layout"] = true;
    data["background_color"] = "#ffff0000";
    data["fps"]["num"] = fps.num;
    data["fps"]["den"] = fps.den;
    Color color("#ff0000");
    color.red.AddPoint(61, 0, LINEAR);
    color.blue.AddPoint(61, 255, LINEAR);
    data["text_animation"]["background"] = color.JsonValue();
    return data;
}
}

TEST_CASE("Text reader info is populated without opening or rendering", "[libopenshot][qttextreader]")
{
    TextTestApplication application;
    QtTextReader reader;
    CHECK_FALSE(reader.IsOpen());
    CHECK(reader.info.width == 1024);
    CHECK(reader.info.height == 768);
    CHECK(reader.info.fps.num == 30);
    Json::Value data = reader.JsonValue();
    data["width"] = 320;
    data["height"] = 40;
    data["fps"]["num"] = 60000;
    data["fps"]["den"] = 1001;
    reader.SetJsonValue(data);
    CHECK_FALSE(reader.IsOpen());
    CHECK(reader.info.width == 320);
    CHECK(reader.info.fps.num == 60000);
    CHECK(reader.info.fps.den == 1001);
    CHECK(reader.info.video_timebase.num == 1001);
    CHECK(reader.info.video_timebase.den == 60000);
    // Empty text still renders a frame of the requested size.
    data["text"] = "";
    reader.SetJsonValue(data);
    reader.Open();
    auto image = reader.GetFrame(1)->GetImage();
    CHECK(image->width() == 320);
    CHECK(image->height() == 40);
    reader.Close();
}

TEST_CASE("Text style curves follow project frames at any frame rate", "[libopenshot][qttextreader]")
{
    TextTestApplication application;
    for (int rate : {24, 30, 60}) {
        CAPTURE(rate);
        QtTextReader reader;
        reader.SetJsonValue(animated_background(reader, Fraction(rate, 1)));
        Clip clip(&reader);
        clip.scale = SCALE_STRETCH;
        clip.End(4.0);
        Timeline timeline(160, 90, Fraction(rate, 1), 44100, 2, LAYOUT_STEREO);
        timeline.AddClip(&clip);
        timeline.Open();
        // Project frame 31 is halfway along curves keyed to project frames.
        auto middle = timeline.GetFrame(31)->GetImage()->pixelColor(80, 45);
        CHECK(std::abs(middle.red() - 128) <= 3);
        CHECK(std::abs(middle.blue() - 128) <= 3);
        auto last = timeline.GetFrame(61)->GetImage()->pixelColor(80, 45);
        CHECK(last.red() <= 2);
        timeline.Close();
    }
}

TEST_CASE("Out-of-order animated frames reuse renders without mixing styles", "[libopenshot][qttextreader]")
{
    TextTestApplication application;
    QtTextReader reader;
    reader.SetJsonValue(animated_background(reader, Fraction(30, 1)));
    reader.Open();
    // Parallel rendering requests frames out of order; each must keep its style.
    for (int pass = 0; pass < 3; ++pass) {
        for (int frame : {61, 1, 31, 61, 1}) {
            CAPTURE(pass, frame);
            auto color = reader.GetFrame(frame)->GetImage()->pixelColor(80, 45);
            const int expected_red = frame == 1 ? 255 : frame == 31 ? 128 : 0;
            CHECK(std::abs(color.red() - expected_red) <= 2);
            CHECK(std::abs(color.blue() - (255 - expected_red)) <= 2);
        }
    }
    // Frames already returned keep their own pixels after later renders.
    auto red = reader.GetFrame(1);
    reader.GetFrame(61);
    CHECK(red->GetImage()->pixelColor(80, 45).red() >= 253);
    reader.Close();
}
