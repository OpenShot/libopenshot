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

SUITE(CVTracker_Tests)
{

TEST(Track_Video)
{
	// Create a video clip
	std::stringstream path;
	path << TEST_MEDIA_PATH << "sintel_trailer-720p.mp4";
	openshot::FFmpegReader c1(path.str());
	c1.Open();

	// Create Tracker
    CVTracker kcfTracker;
	bool trackerInit = false;
	cv::Rect2d lastTrackedBox;

	for (long int frame = 71; frame <= 97; frame++)
    {
		int frame_number = frame;
        std::shared_ptr<openshot::Frame> f = c1.GetFrame(frame_number);
        
        // Grab Mat image
        cv::Mat cvimage = f->GetImageCV();

		if(!trackerInit){
            cv::Rect2d bbox(82, 194, 47, 42);
            kcfTracker.initTracker(bbox, cvimage, frame_number);
            trackerInit = true;
        }
        else{
            trackerInit = kcfTracker.trackFrame(cvimage, frame_number);
			FrameData fd = kcfTracker.GetTrackedData(frame_number);
            cv::Rect2d box(fd.x1, fd.y1, fd.x2-fd.x1, fd.y2-fd.y1);
			lastTrackedBox = box;
		}
	}

	CHECK_EQUAL(27, lastTrackedBox.x);
	CHECK_EQUAL(233, lastTrackedBox.y);
	CHECK_EQUAL(47, lastTrackedBox.width);
	CHECK_EQUAL(42, lastTrackedBox.height);
}


TEST(SaveLoad_Protobuf)
{
	// Create a video clip
	std::stringstream path;
	path << TEST_MEDIA_PATH << "sintel_trailer-720p.mp4";
	Clip c1(path.str());
	c1.Open();

	// Create Tracker
    CVTracker kcfTracker;
	bool trackerInit = false;
	cv::Rect2d lastTrackedBox;

	for (long int frame = 71; frame <= 97; frame++)
    {
		int frame_number = frame;
        std::shared_ptr<openshot::Frame> f = c1.GetFrame(frame_number);
        
        // Grab Mat image
        cv::Mat cvimage = f->GetImageCV();

		if(!trackerInit){
            cv::Rect2d bbox(82, 194, 47, 42);
            kcfTracker.initTracker(bbox, cvimage, frame_number);
            trackerInit = true;
        }
        else{
            trackerInit = kcfTracker.trackFrame(cvimage, frame_number);
			FrameData fd = kcfTracker.GetTrackedData(frame_number);
            cv::Rect2d box(fd.x1, fd.y1, fd.x2-fd.x1, fd.y2-fd.y1);
			lastTrackedBox = box;
		}
	}

	kcfTracker.SaveTrackedData("kcf_tracker.data");

	// Create new tracker
    CVTracker kcfTracker1;
	kcfTracker1.LoadTrackedData("kcf_tracker.data");
	FrameData fd = kcfTracker.GetTrackedData(97);
    cv::Rect2d loadedBox(fd.x1, fd.y1, fd.x2-fd.x1, fd.y2-fd.y1);

	CHECK_EQUAL(loadedBox.x, lastTrackedBox.x);
	CHECK_EQUAL(loadedBox.y, lastTrackedBox.y);
	CHECK_EQUAL(loadedBox.width, lastTrackedBox.width);
	CHECK_EQUAL(loadedBox.height, lastTrackedBox.height);
}

} // SUITE(Frame_Tests)
