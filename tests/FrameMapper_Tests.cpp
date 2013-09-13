/**
 * @file
 * @brief Unit tests for openshot::FrameMapper
 * @author Jonathan Thomas <jonathan@openshot.org>
 *
 * @section LICENSE
 *
 * Copyright (c) 2008-2013 OpenShot Studios, LLC
 * (http://www.openshotstudios.com). This file is part of
 * OpenShot Library (http://www.openshot.org), an open-source project
 * dedicated to delivering high quality video editing and animation solutions
 * to the world.
 *
 * OpenShot Library is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * OpenShot Library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with OpenShot Library.  If not, see <http://www.gnu.org/licenses/>.
 */

#include "UnitTest++.h"
#include "../include/OpenShot.h"

using namespace std;
using namespace openshot;

TEST(FrameMapper_Get_Valid_Frame)
{
	// Create a reader
	DummyReader r(Framerate(24,1), 720, 480, 22000, 2, 5.0);

	// Create mapping between 24 fps and 29.97 fps using classic pulldown
	FrameMapper mapping(&r, Framerate(30000, 1001), PULLDOWN_CLASSIC);

	try
	{
		// Should find this frame
		MappedFrame f = mapping.GetMappedFrame(125);
		CHECK(true); // success
	}
	catch (OutOfBoundsFrame &e)
	{
		// Unexpected failure to find frame
		CHECK(false);
	}
}

TEST(FrameMapper_Invalid_Frame_Too_Small)
{
	// Create a reader
	DummyReader r(Framerate(24,1), 720, 480, 22000, 2, 5.0);

	// Create mapping 24 fps and 29.97 fps
	FrameMapper mapping(&r, Framerate(30000, 1001), PULLDOWN_CLASSIC);

	// Check invalid frame number
	CHECK_THROW(mapping.GetMappedFrame(0), OutOfBoundsFrame);

}

TEST(FrameMapper_Invalid_Frame_Too_Large)
{
	// Create a reader
	DummyReader r(Framerate(24,1), 720, 480, 22000, 2, 4.0);

	// Create mapping 24 fps and 29.97 fps
	FrameMapper mapping(&r, Framerate(30000, 1001), PULLDOWN_CLASSIC);

	// Check invalid frame number
	CHECK_THROW(mapping.GetMappedFrame(151), OutOfBoundsFrame);
}

TEST(FrameMapper_24_fps_to_30_fps_Pulldown_Classic)
{
	// Create a reader
	DummyReader r(Framerate(24,1), 720, 480, 22000, 2, 5.0);

	// Create mapping 24 fps and 29.97 fps
	FrameMapper mapping(&r, Framerate(30000, 1001), PULLDOWN_CLASSIC);
	MappedFrame frame2 = mapping.GetMappedFrame(2);
	MappedFrame frame3 = mapping.GetMappedFrame(3);

	// Check for 3 fields of frame 2
	CHECK_EQUAL(2, frame2.Odd.Frame);
	CHECK_EQUAL(2, frame2.Even.Frame);
	CHECK_EQUAL(2, frame3.Odd.Frame);
	CHECK_EQUAL(3, frame3.Even.Frame);
}

TEST(FrameMapper_24_fps_to_30_fps_Pulldown_Advanced)
{
	// Create a reader
	DummyReader r(Framerate(24,1), 720, 480, 22000, 2, 5.0);

	// Create mapping 24 fps and 29.97 fps
	FrameMapper mapping(&r, Framerate(30000, 1001), PULLDOWN_ADVANCED);
	MappedFrame frame2 = mapping.GetMappedFrame(2);
	MappedFrame frame3 = mapping.GetMappedFrame(3);
	MappedFrame frame4 = mapping.GetMappedFrame(4);

	// Check for advanced pulldown (only 1 fake frame)
	CHECK_EQUAL(2, frame2.Odd.Frame);
	CHECK_EQUAL(2, frame2.Even.Frame);
	CHECK_EQUAL(2, frame3.Odd.Frame);
	CHECK_EQUAL(3, frame3.Even.Frame);
	CHECK_EQUAL(3, frame4.Odd.Frame);
	CHECK_EQUAL(3, frame4.Even.Frame);
}

TEST(FrameMapper_24_fps_to_30_fps_Pulldown_None)
{
	// Create a reader
	DummyReader r(Framerate(24,1), 720, 480, 22000, 2, 5.0);

	// Create mapping 24 fps and 29.97 fps
	FrameMapper mapping(&r, Framerate(30000, 1001), PULLDOWN_NONE);
	MappedFrame frame4 = mapping.GetMappedFrame(4);
	MappedFrame frame5 = mapping.GetMappedFrame(5);

	// Check for advanced pulldown (only 1 fake frame)
	CHECK_EQUAL(4, frame4.Odd.Frame);
	CHECK_EQUAL(4, frame4.Even.Frame);
	CHECK_EQUAL(4, frame5.Odd.Frame);
	CHECK_EQUAL(4, frame5.Even.Frame);
}

TEST(FrameMapper_30_fps_to_24_fps_Pulldown_Classic)
{
	// Create a reader
	DummyReader r(Framerate(30000, 1001), 720, 480, 22000, 2, 5.0);

	// Create mapping between 29.97 fps and 24 fps
	FrameMapper mapping(&r, Framerate(24, 1), PULLDOWN_CLASSIC);
	MappedFrame frame3 = mapping.GetMappedFrame(3);
	MappedFrame frame4 = mapping.GetMappedFrame(4);
	MappedFrame frame5 = mapping.GetMappedFrame(5);

	// Check for advanced pulldown (only 1 fake frame)
	CHECK_EQUAL(4, frame3.Odd.Frame);
	CHECK_EQUAL(3, frame3.Even.Frame);
	CHECK_EQUAL(5, frame4.Odd.Frame);
	CHECK_EQUAL(4, frame4.Even.Frame);
	CHECK_EQUAL(6, frame5.Odd.Frame);
	CHECK_EQUAL(6, frame5.Even.Frame);
}

TEST(FrameMapper_30_fps_to_24_fps_Pulldown_Advanced)
{
	// Create a reader
	DummyReader r(Framerate(30000, 1001), 720, 480, 22000, 2, 5.0);

	// Create mapping between 29.97 fps and 24 fps
	FrameMapper mapping(&r, Framerate(24, 1), PULLDOWN_ADVANCED);
	MappedFrame frame2 = mapping.GetMappedFrame(2);
	MappedFrame frame3 = mapping.GetMappedFrame(3);
	MappedFrame frame4 = mapping.GetMappedFrame(4);

	// Check for advanced pulldown (only 1 fake frame)
	CHECK_EQUAL(2, frame2.Odd.Frame);
	CHECK_EQUAL(2, frame2.Even.Frame);
	CHECK_EQUAL(4, frame3.Odd.Frame);
	CHECK_EQUAL(4, frame3.Even.Frame);
	CHECK_EQUAL(5, frame4.Odd.Frame);
	CHECK_EQUAL(5, frame4.Even.Frame);
}

TEST(FrameMapper_30_fps_to_24_fps_Pulldown_None)
{
	// Create a reader
	DummyReader r(Framerate(30000, 1001), 720, 480, 22000, 2, 5.0);

	// Create mapping between 29.97 fps and 24 fps
	FrameMapper mapping(&r, Framerate(24, 1), PULLDOWN_NONE);
	MappedFrame frame4 = mapping.GetMappedFrame(4);
	MappedFrame frame5 = mapping.GetMappedFrame(5);

	// Check for advanced pulldown (only 1 fake frame)
	CHECK_EQUAL(4, frame4.Odd.Frame);
	CHECK_EQUAL(4, frame4.Even.Frame);
	CHECK_EQUAL(6, frame5.Odd.Frame);
	CHECK_EQUAL(6, frame5.Even.Frame);
}

TEST(FrameMapper_MapTime)
{
	// Create a reader
	DummyReader r(Framerate(24, 1), 720, 480, 22000, 2, 5.0);

	// Create mapping 24 fps and 24 fps
	FrameMapper mapping(&r, Framerate(24, 1), PULLDOWN_NONE);

	// Create a Keyframe to re-map time (forward, reverse, and then forward again)
	Keyframe kf;
	kf.AddPoint(openshot::Point(Coordinate(1, 1), LINEAR));
	kf.AddPoint(openshot::Point(Coordinate(40, 40), LINEAR));
	kf.AddPoint(openshot::Point(Coordinate(60, 20), LINEAR));
	kf.AddPoint(openshot::Point(Coordinate(100, 100), LINEAR));

	// Re-map time
	mapping.MapTime(kf);

	// Check for OutOfBoundsFrames
	CHECK_THROW(mapping.GetMappedFrame(0), OutOfBoundsFrame);
	CHECK_THROW(mapping.GetMappedFrame(101), OutOfBoundsFrame);

	// Check to see if time remapping worked correctly
//	CHECK_EQUAL(1, mapping.GetMappedFrame(1).Odd.Frame);
//	CHECK_EQUAL(2, mapping.GetMappedFrame(2).Odd.Frame);
//	CHECK_EQUAL(39, mapping.GetMappedFrame(39).Odd.Frame);
//	CHECK_EQUAL(40, mapping.GetMappedFrame(40).Odd.Frame);
//	CHECK_EQUAL(39, mapping.GetMappedFrame(41).Odd.Frame);
//	CHECK_EQUAL(38, mapping.GetMappedFrame(42).Odd.Frame);
//	CHECK_EQUAL(21, mapping.GetMappedFrame(59).Odd.Frame);
//	CHECK_EQUAL(20, mapping.GetMappedFrame(60).Odd.Frame);
//	CHECK_EQUAL(22, mapping.GetMappedFrame(61).Odd.Frame);
//	CHECK_EQUAL(24, mapping.GetMappedFrame(62).Odd.Frame);
//	CHECK_EQUAL(90, mapping.GetMappedFrame(95).Odd.Frame);
//	CHECK_EQUAL(100, mapping.GetMappedFrame(100).Odd.Frame);

}
