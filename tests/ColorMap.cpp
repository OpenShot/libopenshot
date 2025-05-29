/**
 * @file
 * @brief Unit tests for ColorMap effect
 * @author Jonathan Thomas
 *
 * @ref License
 */

// Copyright (c) 2008-2025 OpenShot Studios, LLC
//
// SPDX-License-Identifier: LGPL-3.0-or-later

#include <memory>
#include <QImage>
#include <QColor>
#include <sstream>
#include "Frame.h"
#include "effects/ColorMap.h"
#include "openshot_catch.h"

using namespace openshot;

// allow Catch2 to print QColor on failure
static std::ostream& operator<<(std::ostream& os, QColor const& c)
{
    os << "QColor(" << c.red() << "," << c.green()
       << "," << c.blue() << "," << c.alpha() << ")";
    return os;
}

// Build a simple 2×2 frame with one distinct pixel
static std::shared_ptr<Frame> makeTestFrame()
{
    QImage img(2, 2, QImage::Format_ARGB32);
    img.fill(QColor(50,100,150,255));
    img.setPixelColor(0,0, QColor(10,20,30,255));
    auto frame = std::make_shared<Frame>();
    *frame->GetImage() = img;
    return frame;
}

// Helper to construct the LUT-path from TEST_MEDIA_PATH
static std::string lutPath()
{
    std::stringstream path;
    path << TEST_MEDIA_PATH << "example-lut.cube";
    return path.str();
}

TEST_CASE("Default ColorMap with no LUT path leaves image unchanged", "[effect][colormap]")
{
    ColorMap effect;
    auto in = makeTestFrame();
    QColor before = in->GetImage()->pixelColor(0,0);

    auto out = effect.GetFrame(in, 0);
    QColor after = out->GetImage()->pixelColor(0,0);

    CHECK(after == before);
}

TEST_CASE("Overall intensity = 0 leaves image unchanged even when LUT is set", "[effect][colormap]")
{
    ColorMap effect(
        lutPath(),
        Keyframe(0.0), // overall off
        Keyframe(1.0),
        Keyframe(1.0),
        Keyframe(1.0)
    );

    auto in = makeTestFrame();
    QColor before = in->GetImage()->pixelColor(0,0);
    auto out = effect.GetFrame(in, 1);
    QColor after = out->GetImage()->pixelColor(0,0);

    CHECK(after == before);
}

TEST_CASE("JSON round-trip preserves LUT path and intensity keyframe values", "[effect][colormap][json]")
{
    ColorMap A(
        lutPath(),
        Keyframe(0.3), // overall
        Keyframe(0.4),
        Keyframe(0.5),
        Keyframe(0.6)
    );

    std::string serialized = A.Json();
    ColorMap B;
    B.SetJson(serialized);

    CHECK(B.JsonValue()["lut_path"].asString() == lutPath());
    CHECK( B.intensity.  GetValue(0) == Approx(0.3) );
    CHECK( B.intensity_r.GetValue(0) == Approx(0.4) );
    CHECK( B.intensity_g.GetValue(0) == Approx(0.5) );
    CHECK( B.intensity_b.GetValue(0) == Approx(0.6) );
}

TEST_CASE("Clearing LUT path via JSON leaves LUT path empty", "[effect][colormap][json]")
{
    ColorMap effect(
        lutPath(),
        Keyframe(1.0),
        Keyframe(1.0),
        Keyframe(1.0),
        Keyframe(1.0)
    );
    Json::Value clear;
    clear["lut_path"] = std::string("");
    effect.SetJsonValue(clear);

    auto v = effect.JsonValue();
    CHECK(v["lut_path"].asString() == "");
}

TEST_CASE("PropertiesJSON exposes all four intensity properties", "[effect][colormap][ui]")
{
    ColorMap effect;
    std::string props = effect.PropertiesJSON(0);
    Json::CharReaderBuilder rb;
    Json::Value root;
    std::string errs;
    std::istringstream is(props);
    REQUIRE(Json::parseFromStream(rb, is, &root, &errs));

    CHECK(root.isMember("lut_path"));
    CHECK(root.isMember("intensity"));
    CHECK(root.isMember("intensity_r"));
    CHECK(root.isMember("intensity_g"));
    CHECK(root.isMember("intensity_b"));
}

TEST_CASE("Full-intensity LUT changes pixel values", "[effect][colormap][lut]")
{
    ColorMap effect(
        lutPath(),
        Keyframe(1.0), // full overall
        Keyframe(1.0),
        Keyframe(1.0),
        Keyframe(1.0)
    );

    auto in = makeTestFrame();
    QColor before = in->GetImage()->pixelColor(0,0);
    auto out = effect.GetFrame(in, 2);
    QColor after = out->GetImage()->pixelColor(0,0);

    CHECK(after != before);
}

TEST_CASE("Half-intensity LUT changes pixel values less than full-intensity", "[effect][colormap][lut]")
{
    auto in = makeTestFrame();
    QColor before = in->GetImage()->pixelColor(0,0);

    ColorMap half(
        lutPath(),
        Keyframe(0.5), // half overall
        Keyframe(1.0),
        Keyframe(1.0),
        Keyframe(1.0)
    );
    auto out_half = half.GetFrame(in, 3);
    QColor h = out_half->GetImage()->pixelColor(0,0);

    ColorMap full(
        lutPath(),
        Keyframe(1.0),
        Keyframe(1.0),
        Keyframe(1.0),
        Keyframe(1.0)
    );
    auto out_full = full.GetFrame(in, 3);
    QColor f = out_full->GetImage()->pixelColor(0,0);

    int diff_half = std::abs(h.red() - before.red())
                  + std::abs(h.green() - before.green())
                  + std::abs(h.blue() - before.blue());
    int diff_full = std::abs(f.red() - before.red())
                  + std::abs(f.green() - before.green())
                  + std::abs(f.blue() - before.blue());

    CHECK(diff_half < diff_full);
}

TEST_CASE("Disabling red channel produces different result than full-intensity", "[effect][colormap][lut]")
{
    auto in = makeTestFrame();
    QColor before = in->GetImage()->pixelColor(0,0);

    ColorMap full(
        lutPath(),
        Keyframe(1.0),
        Keyframe(1.0),
        Keyframe(1.0),
        Keyframe(1.0)
    );
    auto out_full = full.GetFrame(in, 4);
    QColor f = out_full->GetImage()->pixelColor(0,0);

    ColorMap red_off(
        lutPath(),
        Keyframe(1.0),
        Keyframe(0.0), // red off
        Keyframe(1.0),
        Keyframe(1.0)
    );
    auto out_off = red_off.GetFrame(in, 4);
    QColor r = out_off->GetImage()->pixelColor(0,0);

    CHECK(r != f);
}
