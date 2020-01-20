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

#include "UnitTest++.h"
// Prevent name clashes with juce::UnitTest
#define DONT_SET_USING_JUCE_NAMESPACE 1
#include "../include/OpenShot.h"

#include <QImage>

using namespace openshot;

SUITE(Frame_Tests)
{

TEST(Default_Constructor)
{
	// Create a "blank" default Frame
	std::shared_ptr<Frame> f1(new Frame());

	CHECK(f1 != nullptr);  // Test aborts here if we didn't get a Frame

	// Check basic default parameters
	CHECK_EQUAL(1, f1->GetHeight());
	CHECK_EQUAL(1, f1->GetWidth());
	CHECK_EQUAL(44100, f1->SampleRate());
	CHECK_EQUAL(2, f1->GetAudioChannelsCount());

	// Should be false until we load or create contents
	CHECK_EQUAL(false, f1->has_image_data);
	CHECK_EQUAL(false, f1->has_audio_data);

	// Calling GetImage() paints a blank frame, by default
	std::shared_ptr<QImage> i1 = f1->GetImage();

	CHECK(i1 != nullptr);

	CHECK_EQUAL(true,f1->has_image_data);
	CHECK_EQUAL(false,f1->has_audio_data);
}


TEST(Data_Access)
{
	// Create a video clip
	std::stringstream path;
	path << TEST_MEDIA_PATH << "sintel_trailer-720p.mp4";
	Clip c1(path.str());
	c1.Open();

	// Get first frame
	std::shared_ptr<Frame> f1 = c1.GetFrame(1);

	CHECK(f1 != nullptr);

	CHECK_EQUAL(1, f1->number);
	CHECK_EQUAL(1280, f1->GetWidth());
	CHECK_EQUAL(720, f1->GetHeight());
}


TEST(AddImage_QImage)
{
	// Create a "blank" default Frame
	std::shared_ptr<Frame> f1(new Frame());

	// Load an image
	std::stringstream path;
	path << TEST_MEDIA_PATH << "front.png";
	std::shared_ptr<QImage> i1(new QImage(QString::fromStdString(path.str()))) ;

	CHECK(f1 != nullptr);  // Test aborts here if we didn't get a Frame
	CHECK_EQUAL(false, i1->isNull());

	f1->AddImage(i1);

	// Check loaded image parameters
	CHECK_EQUAL(i1->height(), f1->GetHeight());
	CHECK_EQUAL(i1->width(), f1->GetWidth());
	CHECK_EQUAL(true, f1->has_image_data);
}


TEST(Copy_Constructor)
{
	// Create a dummy Frame
	openshot::Frame f1(1, 800, 600, "#000000");

	// Load an image
	std::stringstream path;
	path << TEST_MEDIA_PATH << "front.png";
	std::shared_ptr<QImage> i1( new QImage(QString::fromStdString(path.str())) );

	CHECK_EQUAL(false, i1->isNull());

	// Add image to f1, then copy f1 to f2
	f1.AddImage(i1);

	Frame f2 = f1;

	CHECK_EQUAL(f1.GetHeight(), f2.GetHeight());
	CHECK_EQUAL(f1.GetWidth(), f2.GetWidth());

	CHECK_EQUAL(f1.has_image_data, f2.has_image_data);
	CHECK_EQUAL(f1.has_audio_data, f2.has_audio_data);

	Fraction par1 = f1.GetPixelRatio();
	Fraction par2 = f2.GetPixelRatio();

	CHECK_EQUAL(par1.num, par2.num);
	CHECK_EQUAL(par1.den, par2.den);


	CHECK_EQUAL(f1.SampleRate(), f2.SampleRate());
	CHECK_EQUAL(f1.GetAudioChannelsCount(), f2.GetAudioChannelsCount());
	CHECK_EQUAL(f1.ChannelsLayout(), f2.ChannelsLayout());

	CHECK_EQUAL(f1.GetBytes(), f2.GetBytes());
	CHECK_EQUAL(f1.GetAudioSamplesCount(), f2.GetAudioSamplesCount());
}

} // SUITE(Frame_Tests)
