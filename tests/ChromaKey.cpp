/**
 * @file
 * @brief Unit tests for openshot::ChromaKey effect
 * @author Jonathan Thomas <jonathan@openshot.org>
 * @author FeRD (Frank Dana) <ferdnyc@gmail.com>
 *
 * @ref License
 */

/* LICENSE
 *
 * Copyright (c) 2008-2021 OpenShot Studios, LLC
 * <http://www.openshotstudios.com/>. This file is part of
 * OpenShot Library (libopenshot), an open-source project dedicated to
 * delivering high quality video editing and animation solutions to the
 * world. For more information visit <http://www.openshot.org/>.
 *
 * OpenShot Library (libopenshot) is free software: you can redistribute it
 * and/or modify it under the terms of the GNU Lesser General Public License
 * as published by the Free Software Foundation, either version 3 of the
 * License, or (at your option) any later version.
 *
 * OpenShot Library (libopenshot) is distributed in the hope that it will be
 * useful, but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public License
 * along with OpenShot Library. If not, see <http://www.gnu.org/licenses/>.
 */

#include <sstream>
#include <memory>

#include <catch2/catch.hpp>

#include "Frame.h"
#include "effects/ChromaKey.h"

#include <QColor>
#include <QImage>

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
    CHECK(pix.red() == 0);
    CHECK(pix.green() == 0);
    CHECK(pix.blue() == 0);
    CHECK(pix.alpha() == 0);
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
    CHECK(pix_e.red() == 0);
    CHECK(pix_e.green() == 204);
    CHECK(pix_e.blue() == 0);
    CHECK(pix_e.alpha() == 255);
}

