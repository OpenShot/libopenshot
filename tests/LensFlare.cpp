/**
 * @file
 * @brief Unit tests for Lens Flare effect
 * @author Jonathan Thomas <jonathan@openshot.org>
 *
 * @ref License
 */

// Copyright (c) 2008-2025 OpenShot Studios, LLC
//
// SPDX-License-Identifier: LGPL-3.0-or-later

#include <memory>
#include <QImage>
#include <QColor>
#include "Frame.h"
#include "effects/LensFlare.h"
#include "openshot_catch.h"

using namespace openshot;

// Allow Catch2 to print QColor on failure
static std::ostream& operator<<(std::ostream& os, QColor const& c)
{
    os << "QColor(" << c.red() << "," << c.green()
       << "," << c.blue() << "," << c.alpha() << ")";
    return os;
}

// Create a small 5×5 grayscale frame
static std::shared_ptr<Frame> makeGrayFrame()
{
    QImage img(5, 5, QImage::Format_ARGB32);
    img.fill(QColor(100, 100, 100, 255));
    auto frame = std::make_shared<Frame>();
    *frame->GetImage() = img;
    return frame;
}

TEST_CASE("LensFlare brightens center pixel", "[effect][lensflare]")
{
    LensFlare effect;
    effect.x          = Keyframe(0.0);
    effect.y          = Keyframe(0.0);
    effect.brightness = Keyframe(1.0);
    effect.size       = Keyframe(1.0);
    effect.spread     = Keyframe(0.0);

    auto frame = makeGrayFrame();
    QColor before = frame->GetImage()->pixelColor(2, 2);

    auto out   = effect.GetFrame(frame, 1);
    QColor after  = out->GetImage()->pixelColor(2, 2);

    CHECK(after != before);
}

TEST_CASE("LensFlare size controls area of effect", "[effect][lensflare]")
{
    LensFlare small, large;
    small.x          = Keyframe(0.0);
    large.x          = Keyframe(0.0);
    small.y          = Keyframe(0.0);
    large.y          = Keyframe(0.0);
    small.brightness = Keyframe(1.0);
    large.brightness = Keyframe(1.0);
    small.spread     = Keyframe(0.0);
    large.spread     = Keyframe(0.0);
    small.size       = Keyframe(0.2);
    large.size       = Keyframe(1.0);

    auto frameSmall = makeGrayFrame();
    auto frameLarge = makeGrayFrame();
    QColor beforeSmall = frameSmall->GetImage()->pixelColor(2, 2);
    QColor beforeLarge = frameLarge->GetImage()->pixelColor(2, 2);

    auto outSmall = small.GetFrame(frameSmall, 1);
    auto outLarge = large.GetFrame(frameLarge, 1);
    QColor afterSmall = outSmall->GetImage()->pixelColor(2, 2);
    QColor afterLarge = outLarge->GetImage()->pixelColor(2, 2);

    CHECK(afterSmall == beforeSmall);
    CHECK(afterLarge != beforeLarge);
}

TEST_CASE("LensFlare brightness scales intensity", "[effect][lensflare]")
{
    LensFlare low, high;
    low.x          = Keyframe(0.0);
    high.x         = Keyframe(0.0);
    low.y          = Keyframe(0.0);
    high.y         = Keyframe(0.0);
    low.size       = Keyframe(1.0);
    high.size      = Keyframe(1.0);
    low.spread     = Keyframe(0.0);
    high.spread    = Keyframe(0.0);
    low.brightness = Keyframe(0.2);
    high.brightness= Keyframe(1.0);

    auto frameLow  = makeGrayFrame();
    auto frameHigh = makeGrayFrame();
    auto outLow    = low.GetFrame(frameLow, 1);
    auto outHigh   = high.GetFrame(frameHigh, 1);
    QColor cLow    = outLow->GetImage()->pixelColor(2, 2);
    QColor cHigh   = outHigh->GetImage()->pixelColor(2, 2);

    CHECK(cLow.red() < cHigh.red());
}