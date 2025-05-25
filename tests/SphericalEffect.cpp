/**
 * @file
 * @brief Unit tests for openshot::SphericalProjection effect using PNG fixtures
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
#include "effects/SphericalProjection.h"
#include "openshot_catch.h"

using namespace openshot;

// allow Catch2 to print QColor on failure
static std::ostream& operator<<(std::ostream& os, QColor const& c)
{
    os << "QColor(" << c.red() << "," << c.green()
        << "," << c.blue() << "," << c.alpha() << ")";
    return os;
}

// load a PNG into a Frame
static std::shared_ptr<Frame> loadFrame(const char* filename)
{
    QImage img(QString(TEST_MEDIA_PATH) + filename);
    img = img.convertToFormat(QImage::Format_ARGB32);
    auto f = std::make_shared<Frame>();
    *f->GetImage() = img;
    return f;
}

// apply effect and sample center pixel
static QColor centerPixel(SphericalProjection& e,
                          std::shared_ptr<Frame> f)
{
    auto img = e.GetFrame(f, 1)->GetImage();
    int cx = img->width() / 2;
    int cy = img->height() / 2;
    return img->pixelColor(cx, cy);
}

TEST_CASE("sphere mode default and invert", "[effect][spherical]")
{
    SphericalProjection e;
    e.projection_mode = 0;
    e.yaw = Keyframe(45.0);

    {
        auto f0 = loadFrame("eq_sphere.png");
        e.invert = 0;
        e.interpolation = 0;
        // eq_sphere.png has green stripe at center
        CHECK(centerPixel(e, f0) == QColor(255,0,0,255));
    }
    {
        auto f1 = loadFrame("eq_sphere.png");
        e.yaw = Keyframe(-45.0);
        e.invert = 0;
        e.interpolation = 1;
        // invert flips view 180°, center maps to blue stripe
        CHECK(centerPixel(e, f1) == QColor(0,0,255,255));
    }
    {
        auto f1 = loadFrame("eq_sphere.png");
        e.yaw = Keyframe(0.0);
        e.invert = 1;
        e.interpolation = 0;
        // invert flips view 180°, center maps to blue stripe
        CHECK(centerPixel(e, f1) == QColor(0,255,0,255));
    }
}

TEST_CASE("hemisphere mode default and invert", "[effect][spherical]")
{
    SphericalProjection e;
    e.projection_mode = 1;

    {
        auto f0 = loadFrame("eq_sphere.png");
        e.yaw = Keyframe(45.0);
        e.invert = 0;
        e.interpolation = 0;
        // hemisphere on full pano still shows green at center
        CHECK(centerPixel(e, f0) == QColor(255,0,0,255));
    }
    {
        auto f1 = loadFrame("eq_sphere.png");
        e.yaw = Keyframe(-45.0);
        e.invert = 0;
        e.interpolation = 1;
        // invert=1 flips center to blue
        CHECK(centerPixel(e, f1) == QColor(0,0,255,255));
    }
    {
        auto f1 = loadFrame("eq_sphere.png");
        e.yaw = Keyframe(-180.0);
        e.invert = 0;
        e.interpolation = 0;
        // invert=1 flips center to blue
        CHECK(centerPixel(e, f1) == QColor(0,255,0,255));
    }
}

TEST_CASE("fisheye mode default and invert", "[effect][spherical]")
{
    SphericalProjection e;
    e.projection_mode = 2;
    e.fov = Keyframe(180.0);

    {
        auto f0 = loadFrame("fisheye.png");
        e.invert = 0;
        e.interpolation = 0;
        // circular mask center remains white
        CHECK(centerPixel(e, f0) == QColor(255,255,255,255));
    }
    {
        auto f1 = loadFrame("fisheye.png");
        e.invert = 1;
        e.interpolation = 1;
        e.fov = Keyframe(90.0);
        // invert has no effect on center
        CHECK(centerPixel(e, f1) == QColor(255,255,255,255));
    }
}

TEST_CASE("fisheye mode yaw has no effect at center", "[effect][spherical]")
{
    SphericalProjection e;
    e.projection_mode = 2;
    e.interpolation = 0;
    e.fov = Keyframe(180.0);
    e.invert = 0;

    auto f = loadFrame("fisheye.png");
    e.yaw = Keyframe(45.0);
    CHECK(centerPixel(e, f) == QColor(255,255,255,255));
}
