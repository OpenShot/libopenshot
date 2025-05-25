/**
* @file
 * @brief Unit tests for Sharpen effect
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
#include "effects/Sharpen.h"
#include "openshot_catch.h"

using namespace openshot;

// allow Catch2 to print QColor on failure
static std::ostream& operator<<(std::ostream& os, QColor const& c)
{
    os << "QColor(" << c.red() << "," << c.green()
       << "," << c.blue() << "," << c.alpha() << ")";
    return os;
}

// Create a tiny 3×720 grayscale frame
static std::shared_ptr<Frame> makeGrayFrame()
{
    QImage img(3, 720, QImage::Format_ARGB32);
    img.fill(QColor(128,128,128,255));
    img.setPixelColor(1,1, QColor(100,100,100,255));
    auto frame = std::make_shared<Frame>();
    *frame->GetImage() = img;
    return frame;
}

// Create a tiny 3×720 colored frame
static std::shared_ptr<Frame> makeColorFrame()
{
    QImage img(3, 720, QImage::Format_ARGB32);
    img.fill(QColor(128,128,128,255));
    img.setPixelColor(1,1, QColor(100,150,200,255));
    auto frame = std::make_shared<Frame>();
    *frame->GetImage() = img;
    return frame;
}

TEST_CASE("zero radius leaves image unchanged", "[effect][sharpen]")
{
    Sharpen effect;
    effect.amount    = Keyframe(1.0);
    effect.radius    = Keyframe(0.0);
    effect.threshold = Keyframe(1.0);

    auto frame = makeGrayFrame();
    QColor before = frame->GetImage()->pixelColor(1,1);

    auto out = effect.GetFrame(frame, 1);
    QColor after = out->GetImage()->pixelColor(1,1);

    CHECK(after == before);
}

TEST_CASE("nonzero radius and threshold sharpens tiny grayscale image", "[effect][sharpen]")
{
    Sharpen effect;
    effect.amount    = Keyframe(1.0);
    effect.radius    = Keyframe(1.0);
    effect.threshold = Keyframe(1.0);

    auto frame = makeGrayFrame();
    QColor before = frame->GetImage()->pixelColor(1,1);

    auto out = effect.GetFrame(frame, 1);
    QColor after = out->GetImage()->pixelColor(1,1);

    CHECK(after != before);
}

TEST_CASE("zero amount leaves image unchanged", "[effect][sharpen]")
{
    Sharpen effect;
    effect.amount    = Keyframe(0.0);
    effect.radius    = Keyframe(1.0);
    effect.threshold = Keyframe(1.0);

    auto frame = makeGrayFrame();
    QColor before = frame->GetImage()->pixelColor(1,1);

    auto out = effect.GetFrame(frame, 1);
    QColor after = out->GetImage()->pixelColor(1,1);

    CHECK(after == before);
}

TEST_CASE("HighPass vs UnsharpMask produce distinct results on grayscale", "[effect][sharpen]")
{
    Sharpen usm, hp;
    usm.amount    = Keyframe(2.0);
    usm.radius    = Keyframe(1.0);
    usm.threshold = Keyframe(0.0);
    usm.mode      = 0; // UnsharpMask

    hp = usm;
    hp.mode = 1;       // HighPassBlend

    auto f1 = makeGrayFrame();
    auto f2 = makeGrayFrame();

    QColor out_usm = usm.GetFrame(f1,1)->GetImage()->pixelColor(1,1);
    QColor out_hp  = hp .GetFrame(f2,1)->GetImage()->pixelColor(1,1);

    CHECK(out_hp != out_usm);
}

TEST_CASE("Luma-only differs from All on colored image", "[effect][sharpen]")
{
    Sharpen allc, lumac;
    allc.amount    = Keyframe(2.0);
    allc.radius    = Keyframe(1.0);
    allc.threshold = Keyframe(0.0);
    allc.mode      = 0;
    allc.channel   = 0; // All

    lumac = allc;
    lumac.channel = 1;  // Luma only

    auto f_all  = makeColorFrame();
    auto f_luma = makeColorFrame();

    QColor out_all  = allc .GetFrame(f_all, 1)->GetImage()->pixelColor(1,1);
    QColor out_luma = lumac.GetFrame(f_luma,1)->GetImage()->pixelColor(1,1);

    CHECK(out_luma != out_all);
}

TEST_CASE("Chroma-only differs from All on colored image", "[effect][sharpen]")
{
    Sharpen allc, chromac;
    allc.amount    = Keyframe(2.0);
    allc.radius    = Keyframe(1.0);
    allc.threshold = Keyframe(0.0);
    allc.mode      = 0;
    allc.channel   = 0; // All

    chromac = allc;
    chromac.channel = 2; // Chroma only

    auto f_all    = makeColorFrame();
    auto f_chroma = makeColorFrame();

    QColor out_all    = allc  .GetFrame(f_all,    1)->GetImage()->pixelColor(1,1);
    QColor out_chroma = chromac.GetFrame(f_chroma,1)->GetImage()->pixelColor(1,1);

    CHECK(out_chroma != out_all);
}
