/**
 * @file
 * @brief Unit tests for openshot::ChromaKey effect
 * @author Jonathan Thomas <jonathan@openshot.org>
 * @author FeRD (Frank Dana) <ferdnyc@gmail.com>
 *
 * @ref License
 */

// Copyright (c) 2008-2021 OpenShot Studios, LLC
//
// SPDX-License-Identifier: LGPL-3.0-or-later

#include <sstream>
#include <memory>

#include "Frame.h"
#include "effects/ChromaKey.h"

#include <QColor>
#include <QImage>

// Stream output formatter for QColor, needed so Catch2 can display
// values when CHECK(qcolor1 == qcolor2) comparisons fail
std::ostream& operator << ( std::ostream& os, QColor const& value ) {
    os << "QColor(" << value.red() << ", " << value.green() << ", "
       << value.blue() << ", " << value.alpha() << ")";
    return os;
}

#include "openshot_catch.h"

using namespace openshot;

TEST_CASE( "basic keying", "[libopenshot][effect][chromakey]" )
{
    // solid green frame
    auto f = std::make_shared<openshot::Frame>(1, 1280, 720, "#00ff00");

    // Create a ChromaKey effect to key on solid green ± 5 values
    openshot::Color key(0, 255, 0, 255);
    openshot::Keyframe fuzz(5);
    openshot::ChromaKey e(key, fuzz);

    auto f_out = e.GetFrame(f, 1);
    std::shared_ptr<QImage> i = f_out->GetImage();

    // Check color fill (should be transparent)
    QColor pix = i->pixelColor(10, 10);
    QColor trans{Qt::transparent};
    CHECK(pix == trans);
}

TEST_CASE( "threshold", "[libopenshot][effect][chromakey]" )
{
    auto frame = std::make_shared<openshot::Frame>(1, 1280, 720, "#00cc00");

    // Create a ChromaKey effect to key on solid green ± 5 values
    openshot::Color key(0, 255, 0, 255);
    openshot::Keyframe fuzz(5);
    openshot::ChromaKey e(key, fuzz);

    auto frame_out = e.GetFrame(frame, 1);
    std::shared_ptr<QImage> i = frame_out->GetImage();

    // Output should be the same, no ChromaKey
    QColor pix_e = i->pixelColor(10, 10);
    QColor expected(0, 204, 0, 255);
    CHECK(pix_e == expected);
}

