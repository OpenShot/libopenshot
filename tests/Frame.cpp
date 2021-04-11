/**
 * @file
 * @brief Unit tests for openshot::Frame
 * @author Jonathan Thomas <jonathan@openshot.org>
 * @author FeRD (Frank Dana) <ferdnyc@gmail.com>
 *
 * @ref License
 */

/* LICENSE
 *
 * Copyright (c) 2008-2019 OpenShot Studios, LLC
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

#include <QImage>

#ifdef USE_OPENCV
#define int64 opencv_broken_int
#define uint64 opencv_broken_uint
#include <opencv2/opencv.hpp>
#undef int64
#undef uint64
#endif

#include <catch2/catch.hpp>

#include "Clip.h"
#include "Fraction.h"
#include "Frame.h"

using namespace openshot;

TEST_CASE( "Default_Constructor", "[libopenshot][frame]" )
{
	// Create a "blank" default Frame
	std::shared_ptr<Frame> f1(new Frame());

	REQUIRE(f1 != nullptr);  // Test aborts here if we didn't get a Frame

	// Check basic default parameters
	CHECK(f1->GetHeight() == 1);
	CHECK(f1->GetWidth() == 1);
	CHECK(f1->SampleRate() == 44100);
	CHECK(f1->GetAudioChannelsCount() == 2);

	// Should be false until we load or create contents
	CHECK(f1->has_image_data == false);
	CHECK(f1->has_audio_data == false);

	// Calling GetImage() paints a blank frame, by default
	std::shared_ptr<QImage> i1 = f1->GetImage();

	REQUIRE(i1 != nullptr);

	CHECK(f1->has_image_data == true);
	CHECK(f1->has_audio_data == false);
}


TEST_CASE( "Data_Access", "[libopenshot][frame]" )
{
	// Create a video clip
	std::stringstream path;
	path << TEST_MEDIA_PATH << "sintel_trailer-720p.mp4";
	Clip c1(path.str());
	c1.Open();

	// Get first frame
	std::shared_ptr<Frame> f1 = c1.GetFrame(1);

	REQUIRE(f1 != nullptr);

	CHECK(f1->number == 1);
	CHECK(f1->GetWidth() == 1280);
	CHECK(f1->GetHeight() == 720);
}


TEST_CASE( "AddImage_QImage", "[libopenshot][frame]" )
{
	// Create a "blank" default Frame
	std::shared_ptr<Frame> f1(new Frame());

	// Load an image
	std::stringstream path;
	path << TEST_MEDIA_PATH << "front.png";
	auto i1 = std::make_shared<QImage>(QString::fromStdString(path.str()));

	REQUIRE(f1 != nullptr);  // Test aborts here if we didn't get a Frame
	CHECK(i1->isNull() == false);

	f1->AddImage(i1);

	// Check loaded image parameters
	CHECK(f1->GetHeight() == i1->height());
	CHECK(f1->GetWidth() == i1->width());
	CHECK(f1->has_image_data == true);
}


TEST_CASE( "Copy_Constructor", "[libopenshot][frame]" )
{
	// Create a dummy Frame
	openshot::Frame f1(1, 800, 600, "#000000");

	// Load an image
	std::stringstream path;
	path << TEST_MEDIA_PATH << "front.png";
	auto i1 = std::make_shared<QImage>(QString::fromStdString(path.str()));

	CHECK(i1->isNull() == false);

	// Add image to f1, then copy f1 to f2
	f1.AddImage(i1);

	Frame f2 = f1;

	CHECK(f1.GetHeight() == f2.GetHeight());
	CHECK(f1.GetWidth() == f2.GetWidth());

	CHECK(f1.has_image_data == f2.has_image_data);
	CHECK(f1.has_audio_data == f2.has_audio_data);

	Fraction par1 = f1.GetPixelRatio();
	Fraction par2 = f2.GetPixelRatio();

	CHECK(par1.num == par2.num);
	CHECK(par1.den == par2.den);


	CHECK(f1.SampleRate() == f2.SampleRate());
	CHECK(f1.GetAudioChannelsCount() == f2.GetAudioChannelsCount());
	CHECK(f1.ChannelsLayout() == f2.ChannelsLayout());

	CHECK(f1.GetBytes() == f2.GetBytes());
	CHECK(f1.GetAudioSamplesCount() == f2.GetAudioSamplesCount());
}

#ifdef USE_OPENCV
TEST_CASE( "Convert_Image", "[libopenshot][opencv][frame]" )
{
	// Create a video clip
	std::stringstream path;
	path << TEST_MEDIA_PATH << "sintel_trailer-720p.mp4";
	Clip c1(path.str());
	c1.Open();

	// Get first frame
	auto f1 = c1.GetFrame(1);

	// Get first Mat image
	cv::Mat cvimage = f1->GetImageCV();

	CHECK_FALSE(cvimage.empty());

	CHECK(f1->number == 1);
	CHECK(f1->GetWidth() == cvimage.cols);
	CHECK(f1->GetHeight() == cvimage.rows);
	CHECK(cvimage.channels() == 3);
}
#endif
