/**
 * @file
 * @brief Unit tests for openshot::Crop effect
 * @author Jonathan Thomas <jonathan@openshot.org>
 * @author FeRD (Frank Dana) <ferdnyc@gmail.com>
 *
 * @ref License
 */

// Copyright (c) 2008-2021 OpenShot Studios, LLC
//
// SPDX-License-Identifier: LGPL-3.0-or-later

#include <memory>

#include "openshot_catch.h"

#include "Frame.h"
#include "effects/Crop.h"

#include <QColor>
#include <QImage>
#include <QPainter>
#include <QSize>

TEST_CASE( "default constructor", "[libopenshot][effect][crop]" )
{
    // solid green frame
    auto f = std::make_shared<openshot::Frame>(1, 1280, 720, "#00ff00");

    // Default constructor should have no cropping
    openshot::Crop e;

    auto f_out = e.GetFrame(f, 1);
    std::shared_ptr<QImage> i = f_out->GetImage();

    // Check pixels near edges (should all be green)
    std::vector<QColor> pixels {
        i->pixelColor(400, 2),
        i->pixelColor(1279, 500),
        i->pixelColor(800, 718),
        i->pixelColor(1, 200)
    };

    QColor green{Qt::green};
    CHECK(pixels[0] == green);
    CHECK(pixels[1] == green);
    CHECK(pixels[2] == green);
    CHECK(pixels[3] == green);
}

TEST_CASE( "basic cropping", "[libopenshot][effect][crop]" )
{
    auto frame = std::make_shared<openshot::Frame>(1, 1280, 720, "#00ff00");

    // Crop 10% off the input frame on all four sides
    openshot::Keyframe left(0.1);
    openshot::Keyframe top(0.1);
    openshot::Keyframe right(0.1);
    openshot::Keyframe bottom(0.1);
    openshot::Crop e(left, top, right, bottom);

    auto frame_out = e.GetFrame(frame, 1);
    std::shared_ptr<QImage> i = frame_out->GetImage();

    QSize sz(1280, 720);
    CHECK(i->size() == sz);

    // Green inside the crop region, transparent outside
    QColor green{Qt::green};
    QColor trans{Qt::transparent};

    QColor center_pixel = i->pixelColor(640, 360);
    CHECK(center_pixel == green);

    std::vector<QColor> edge_pixels {
        i->pixelColor(50, 200),
        i->pixelColor(400, 20),
        i->pixelColor(1250, 500),
        i->pixelColor(800, 715)
    };
    CHECK(edge_pixels[0] == trans);
    CHECK(edge_pixels[1] == trans);
    CHECK(edge_pixels[2] == trans);
    CHECK(edge_pixels[3] == trans);
}

TEST_CASE( "region collapsing", "[libopenshot][effect][crop]" )
{
    auto frame = std::make_shared<openshot::Frame>(1, 1920, 1080, "#ff00ff");

    // Crop 50% off left and right sides (== crop out entire image)
    openshot::Keyframe left(0.4);
    openshot::Keyframe right(0.6);
    openshot::Keyframe none(0.0);
    openshot::Crop e(left, none, right, none);

    auto frame_out = e.GetFrame(frame, 1);
    auto i = frame_out->GetImage();

    // Only true if all pixels have been cropped away (as expected)
    CHECK(i->allGray());
}

TEST_CASE( "x/y offsets", "[libopenshot][effect][crop]" )
{
    auto frame = std::make_shared<openshot::Frame>(1, 1280, 720, "#ff0000");
    auto frame_img = frame->GetImage();
    QImage img(*frame_img);

    // Make input frame left-half red, right-half blue
    QPainter p(&img);
    p.fillRect(QRect(640, 0, 640, 720), Qt::blue);
    p.end();

    frame->AddImage(std::make_shared<QImage>(img));

    // Crop 20% off all four sides, and shift the source window x +33⅓ %
    openshot::Keyframe sides(0.2);
    openshot::Keyframe x(0.3);
    openshot::Crop e(sides, sides, sides, sides, x);

    auto frame_out = e.GetFrame(frame, 1);
    std::shared_ptr<QImage> i = frame_out->GetImage();

    // Entire cropped region should be blue (due to x-offset), and will be
    // off-center (due to being only 50% wide instead of 60%)
    QColor blue{Qt::blue};
    QColor trans{Qt::transparent};

    std::vector<QColor> edge_pixels {
        i->pixelColor(258, 146),
        i->pixelColor(894, 146),
        i->pixelColor(894, 574),
        i->pixelColor(258, 574)
    };
    CHECK(edge_pixels[0] == blue);
    CHECK(edge_pixels[1] == blue);
    CHECK(edge_pixels[2] == blue);
    CHECK(edge_pixels[3] == blue);

    // This pixel would normally be inside the cropping.
    // The x-offset moves it outside of the source image area,
    // so it becomes a transparent pixel
    CHECK(i->pixelColor(900, 360) == trans);
}
