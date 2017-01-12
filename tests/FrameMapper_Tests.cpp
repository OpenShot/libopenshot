/**
 * @file
 * @brief Unit tests for openshot::FrameMapper
 * @author Jonathan Thomas <jonathan@openshot.org>
 *
 * @section LICENSE
 *
 * Copyright (c) 2008-2014 OpenShot Studios, LLC
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
#include "../include/OpenShot.h"
#include "../include/Tests.h"

using namespace std;
using namespace openshot;

TEST(FrameMapper_Get_Valid_Frame)
{
	// Create a reader
	DummyReader r(Fraction(24,1), 720, 480, 22000, 2, 5.0);

	// Create mapping between 24 fps and 29.97 fps using classic pulldown
	FrameMapper mapping(&r, Fraction(30000, 1001), PULLDOWN_CLASSIC, 22000, 2, LAYOUT_STEREO);

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
	DummyReader r(Fraction(24,1), 720, 480, 22000, 2, 5.0);

	// Create mapping 24 fps and 29.97 fps
	FrameMapper mapping(&r, Fraction(30000, 1001), PULLDOWN_CLASSIC, 22000, 2, LAYOUT_STEREO);

	// Check invalid frame number
	CHECK_THROW(mapping.GetMappedFrame(0), OutOfBoundsFrame);

}

TEST(FrameMapper_24_fps_to_30_fps_Pulldown_Classic)
{
	// Create a reader
	DummyReader r(Fraction(24,1), 720, 480, 22000, 2, 5.0);

	// Create mapping 24 fps and 30 fps
	FrameMapper mapping(&r, Fraction(30, 1), PULLDOWN_CLASSIC, 22000, 2, LAYOUT_STEREO);
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
	DummyReader r(Fraction(24,1), 720, 480, 22000, 2, 5.0);

	// Create mapping 24 fps and 30 fps
	FrameMapper mapping(&r, Fraction(30, 1), PULLDOWN_ADVANCED, 22000, 2, LAYOUT_STEREO);
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
	DummyReader r(Fraction(24,1), 720, 480, 22000, 2, 5.0);

	// Create mapping 24 fps and 30 fps
	FrameMapper mapping(&r, Fraction(30, 1), PULLDOWN_NONE, 22000, 2, LAYOUT_STEREO);
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
	DummyReader r(Fraction(30, 1), 720, 480, 22000, 2, 5.0);

	// Create mapping between 30 fps and 24 fps
	FrameMapper mapping(&r, Fraction(24, 1), PULLDOWN_CLASSIC, 22000, 2, LAYOUT_STEREO);
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
	DummyReader r(Fraction(30, 1), 720, 480, 22000, 2, 5.0);

	// Create mapping between 30 fps and 24 fps
	FrameMapper mapping(&r, Fraction(24, 1), PULLDOWN_ADVANCED, 22000, 2, LAYOUT_STEREO);
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
	DummyReader r(Fraction(30, 1), 720, 480, 22000, 2, 5.0);

	// Create mapping between 30 fps and 24 fps
	FrameMapper mapping(&r, Fraction(24, 1), PULLDOWN_NONE, 22000, 2, LAYOUT_STEREO);
	MappedFrame frame4 = mapping.GetMappedFrame(4);
	MappedFrame frame5 = mapping.GetMappedFrame(5);

	// Check for advanced pulldown (only 1 fake frame)
	CHECK_EQUAL(4, frame4.Odd.Frame);
	CHECK_EQUAL(4, frame4.Even.Frame);
	CHECK_EQUAL(6, frame5.Odd.Frame);
	CHECK_EQUAL(6, frame5.Even.Frame);
}

TEST(FrameMapper_resample_audio_48000_to_41000)
{
	// Create a reader: 24 fps, 2 channels, 48000 sample rate
	stringstream path;
	path << TEST_MEDIA_PATH << "sintel_trailer-720p.mp4";
	FFmpegReader r(path.str());

	// Map to 30 fps, 3 channels surround, 44100 sample rate
	FrameMapper map(&r, Fraction(30,1), PULLDOWN_NONE, 44100, 3, LAYOUT_SURROUND);
	map.Open();

	// Check details
	CHECK_EQUAL(3, map.GetFrame(1)->GetAudioChannelsCount());
	CHECK_EQUAL(1470, map.GetFrame(1)->GetAudioSamplesCount());
	CHECK_EQUAL(1470, map.GetFrame(2)->GetAudioSamplesCount());
	CHECK_EQUAL(1470, map.GetFrame(50)->GetAudioSamplesCount());

	// Change mapping data
	map.ChangeMapping(Fraction(25,1), PULLDOWN_NONE, 22050, 1, LAYOUT_MONO);

	// Check details
	CHECK_EQUAL(1, map.GetFrame(1)->GetAudioChannelsCount());
	CHECK_EQUAL(882, map.GetFrame(1)->GetAudioSamplesCount());
	CHECK_EQUAL(882, map.GetFrame(2)->GetAudioSamplesCount());
	CHECK_EQUAL(882, map.GetFrame(50)->GetAudioSamplesCount());

	// Close mapper
	map.Close();
}
