/**
 * @file
 * @brief Unit tests for openshot::Frame
 * @author Jonathan Thomas <jonathan@openshot.org>
 * @author FeRD (Frank Dana) <ferdnyc@gmail.com>
 *
 * @ref License
 */

// Copyright (c) 2008-2019 OpenShot Studios, LLC
//
// SPDX-License-Identifier: LGPL-3.0-or-later

#include <sstream>
#include <memory>
#include <cstdio>
#include <cmath>

#include <QImage>
#include <QColor>

#ifdef USE_OPENCV
#define int64 opencv_broken_int
#define uint64 opencv_broken_uint
#include <opencv2/opencv.hpp>
#undef int64
#undef uint64
#endif

#include "openshot_catch.h"

#include "Clip.h"
#include "Fraction.h"
#include "Frame.h"

using namespace openshot;

namespace {

std::shared_ptr<Frame> make_vertical_stripe_frame() {
	auto frame = std::make_shared<Frame>(1, 100, 300, "#000000");
	auto image = std::make_shared<QImage>(100, 300, QImage::Format_RGBA8888);

	for (int y = 0; y < image->height(); ++y) {
		QColor color = Qt::red;
		if (y >= 100 && y < 200)
			color = Qt::green;
		else if (y >= 200)
			color = Qt::blue;

		for (int x = 0; x < image->width(); ++x)
			image->setPixelColor(x, y, color);
	}

	frame->AddImage(image);
	return frame;
}

QImage write_thumbnail(
	const std::shared_ptr<Frame>& frame,
	const std::string& path,
	bool ignore_aspect,
	ScaleType scale_mode = SCALE_FIT) {
	frame->Thumbnail(path, 98, 64, "", "", "#000000", ignore_aspect, "PNG", 100, 0.0f, scale_mode);
	QImage result(QString::fromStdString(path));
	std::remove(path.c_str());
	return result;
}

void check_thumbnail_dimensions(const QImage& image) {
	REQUIRE_FALSE(image.isNull());
	CHECK(image.width() == 98);
	CHECK(image.height() == 64);
}

}

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
	CHECK(std::isnan(f1->capture_timestamp));

	// Calling GetImage() paints a blank frame, by default
	std::shared_ptr<QImage> i1 = f1->GetImage();

	REQUIRE(i1 != nullptr);

	CHECK(f1->has_image_data == true);
	CHECK(f1->has_audio_data == false);
}

TEST_CASE( "Capture_Timestamp_Copies", "[libopenshot][frame]" )
{
	openshot::Frame f1(1, 800, 600, "#000000");
	f1.capture_timestamp = 12.345;

	openshot::Frame f2(f1);
	CHECK(f2.capture_timestamp == Approx(12.345));

	openshot::Frame f3;
	f3.DeepCopy(f1);
	CHECK(f3.capture_timestamp == Approx(12.345));
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

TEST_CASE( "GetSamplesPerFrame invalid rate inputs", "[libopenshot][frame]" )
{
	CHECK(Frame::GetSamplesPerFrame(/*frame_number=*/1, Fraction(0, 1), /*sample_rate=*/44100, /*channels=*/2) == 0);
	CHECK(Frame::GetSamplesPerFrame(/*frame_number=*/1, Fraction(30, 0), /*sample_rate=*/44100, /*channels=*/2) == 0);
	CHECK(Frame::GetSamplesPerFrame(/*frame_number=*/1, Fraction(30, 1), /*sample_rate=*/0, /*channels=*/2) == 0);
	CHECK(Frame::GetSamplesPerFrame(/*frame_number=*/1, Fraction(30, 1), /*sample_rate=*/44100, /*channels=*/0) == 0);
}

TEST_CASE( "Thumbnail preserves legacy fit behavior without scale mode", "[libopenshot][frame][thumbnail]" )
{
	std::stringstream thumb_path;
	thumb_path << TEST_MEDIA_PATH << "thumbnail-fit-default-test.png";
	auto result = write_thumbnail(make_vertical_stripe_frame(), thumb_path.str(), false);

	check_thumbnail_dimensions(result);

	QColor left = result.pixelColor(0, result.height() / 2);
	QColor center = result.pixelColor(result.width() / 2, result.height() / 2);
	QColor right = result.pixelColor(result.width() - 1, result.height() / 2);

	CHECK(left == QColor(Qt::black));
	CHECK(center == QColor(Qt::green));
	CHECK(right == QColor(Qt::black));
}

TEST_CASE( "Thumbnail explicit fit scale mode matches legacy behavior", "[libopenshot][frame][thumbnail]" )
{
	std::stringstream thumb_path;
	thumb_path << TEST_MEDIA_PATH << "thumbnail-fit-explicit-test.png";
	auto result = write_thumbnail(make_vertical_stripe_frame(), thumb_path.str(), false, SCALE_FIT);

	check_thumbnail_dimensions(result);

	QColor left = result.pixelColor(0, result.height() / 2);
	QColor center = result.pixelColor(result.width() / 2, result.height() / 2);
	QColor right = result.pixelColor(result.width() - 1, result.height() / 2);

	CHECK(left == QColor(Qt::black));
	CHECK(center == QColor(Qt::green));
	CHECK(right == QColor(Qt::black));
}

TEST_CASE( "Thumbnail scale mode supports crop", "[libopenshot][frame][thumbnail]" )
{
	std::stringstream thumb_path;
	thumb_path << TEST_MEDIA_PATH << "thumbnail-crop-test.png";
	auto result = write_thumbnail(make_vertical_stripe_frame(), thumb_path.str(), false, SCALE_CROP);

	check_thumbnail_dimensions(result);

	QColor left = result.pixelColor(0, result.height() / 2);
	QColor center = result.pixelColor(result.width() / 2, result.height() / 2);
	QColor right = result.pixelColor(result.width() - 1, result.height() / 2);

	CHECK(left == QColor(Qt::green));
	CHECK(center == QColor(Qt::green));
	CHECK(right == QColor(Qt::green));
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
