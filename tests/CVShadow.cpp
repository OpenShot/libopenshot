/**
 * @file
 * @brief Unit tests for OpenCV Outline effect
 * @author Jonathan Thomas <jonathan@openshot.org>
 *
 * @ref License
 */

// Copyright (c) 2008-2025 OpenShot Studios, LLC
//
// SPDX-License-Identifier: LGPL-3.0-or-later

#include <sstream>
#include <memory>
#include <cmath>

#include "openshot_catch.h"

#include "Clip.h"
#include "effects/Shadow.h"

using namespace openshot;

TEST_CASE( "Shadow_Tests", "[libopenshot][opencv][shadow]" )
{
    // Create a video clip
    std::stringstream path;
    path << TEST_MEDIA_PATH << "1F0CF.svg";

    // Open clip
    openshot::Clip c(path.str());
    c.Open();
    auto f = c.GetFrame(1);

    // Create effect constructor (default values)
    openshot::Shadow e1{};

    // Get frame from effect
    auto f1 = e1.GetFrame(f, 1);
    std::shared_ptr<QImage> i1 = f1->GetImage();

    // Check effect colors
    QColor pix1 = i1->pixelColor(3, 32);
    QColor compare1{0, 0, 0, 0};
    CHECK(pix1 == compare1);

    // Test another effect constructor
    // How to check color blending on shadow???

    // Close clip
    c.Close();
}
