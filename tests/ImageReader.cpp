/**
 * @file
 * @brief Unit tests for openshot::ImageReader
 * @author Jonathan Thomas <jonathan@openshot.org>
 *
 * @ref License
 */

// Copyright (c) 2008-2025 OpenShot Studios, LLC
//
// SPDX-License-Identifier: LGPL-3.0-or-later

#include "openshot_catch.h"

#include "ImageReader.h"
#include "Exceptions.h"
#include "Frame.h"
#include <sstream>
#include <cstring>

using namespace openshot;

TEST_CASE( "Invalid_Path_ImageReader", "[libopenshot][imagereader]" )
{
	// Check invalid path and error details
	const std::string invalid_path = "/tmp/__openshot_missing_test_file__.png";
	try {
		ImageReader r(invalid_path);
		FAIL("Expected InvalidFile for missing image path");
	} catch (const InvalidFile& e) {
		const std::string message = e.what();
		CHECK(message.find("ImageReader could not open image file.") != std::string::npos);
		CHECK(message.find(invalid_path) != std::string::npos);
	}
}


TEST_CASE( "Cached_Frames_ImageReader",
           "[libopenshot][imagereader]" )
{
    std::stringstream path;
    path << TEST_MEDIA_PATH << "front.png";

    ImageReader reader(path.str());
    reader.Open();

    auto first = reader.GetFrame(1);
    auto second = reader.GetFrame(30);

    REQUIRE(first != nullptr);
    REQUIRE(second != nullptr);

    CHECK(first->GetWidth() == second->GetWidth());
    CHECK(first->GetHeight() == second->GetHeight());

    const unsigned char* first_pixels = first->GetPixels();
    const unsigned char* second_pixels = second->GetPixels();

    REQUIRE(first_pixels != nullptr);
    REQUIRE(second_pixels != nullptr);

    const size_t pixel_count =
        static_cast<size_t>(first->GetWidth()) *
        static_cast<size_t>(first->GetHeight()) * 4;

    CHECK(std::memcmp(first_pixels, second_pixels,
                      pixel_count) == 0);

    reader.Close();
    CHECK_FALSE(reader.IsOpen());

    reader.Open();
    CHECK(reader.IsOpen());

    auto reopened = reader.GetFrame(1);
    REQUIRE(reopened != nullptr);

    CHECK(reopened->GetWidth() == first->GetWidth());
    CHECK(reopened->GetHeight() == first->GetHeight());

    reader.Close();
}

TEST_CASE( "Cached_Frame_Independence_ImageReader",
           "[libopenshot][imagereader]" )
{
    std::stringstream path;
    path << TEST_MEDIA_PATH << "front.png";

    ImageReader reader(path.str());
    reader.Open();

    auto first = reader.GetFrame(1);
    auto second = reader.GetFrame(2);

    REQUIRE(first != nullptr);
    REQUIRE(second != nullptr);

    auto first_image = first->GetImage();
    auto second_image = second->GetImage();

    REQUIRE(first_image != nullptr);
    REQUIRE(second_image != nullptr);

    REQUIRE_FALSE(first_image->isNull());
    REQUIRE(first_image->size() == second_image->size());

    const QRgb original = second_image->pixel(0, 0);
    const QRgb modified = original ^ 0x00FFFFFF;

    // Qt detaches the first QImage on mutation.
    first_image->setPixel(0, 0, modified);

    CHECK(first_image->pixel(0, 0) == modified);
    CHECK(second_image->pixel(0, 0) == original);

    // The cached source must also remain unchanged.
    auto third = reader.GetFrame(3);
    REQUIRE(third != nullptr);
    CHECK(third->GetImage()->pixel(0, 0) == original);

    reader.Close();

    // Returned frames must outlive the reader's cache.
    CHECK(second_image->pixel(0, 0) == original);
    CHECK(third->GetImage()->pixel(0, 0) == original);
}

TEST_CASE( "Duration_And_Length_ImageReader", "[libopenshot][imagereader]" )
{
	// Create a reader
	std::stringstream path;
	path << TEST_MEDIA_PATH << "front.png";
	ImageReader r(path.str());
	r.Open();

	// Duration and frame count should be aligned to fps (1 hour at 30 fps)
	CHECK(r.info.fps.num == 30);
	CHECK(r.info.fps.den == 1);
	CHECK(r.info.video_length == 108000);
	CHECK(r.info.duration == Approx(3600.0f).margin(0.001f));

	r.Close();
}
