#include "UnitTest++.h"
#include "../include/OpenShot.h"

using namespace std;
using namespace openshot;

TEST(FrameMapper_Get_Valid_Frame)
{
	// Create mapping between 24 fps and 29.97 fps using classic pulldown
	FrameMapper mapping(100, Framerate(24, 1), Framerate(30000, 1001), PULLDOWN_CLASSIC);

	try
	{
		// Should find this frame
		MappedFrame f = mapping.GetFrame(125);
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
	// Create mapping 24 fps and 29.97 fps
	FrameMapper mapping(100, Framerate(24, 1), Framerate(30000, 1001), PULLDOWN_CLASSIC);

	// Check invalid frame number
	CHECK_THROW(mapping.GetFrame(0), OutOfBoundsFrame);

}

TEST(FrameMapper_Invalid_Frame_Too_Large)
{
	// Create mapping 24 fps and 29.97 fps
	FrameMapper mapping(100, Framerate(24, 1), Framerate(30000, 1001), PULLDOWN_CLASSIC);

	// Check invalid frame number
	CHECK_THROW(mapping.GetFrame(126), OutOfBoundsFrame);
}

TEST(FrameMapper_24_fps_to_30_fps_Pulldown_Classic)
{
	// Create mapping 24 fps and 29.97 fps
	FrameMapper mapping(100, Framerate(24, 1), Framerate(30000, 1001), PULLDOWN_CLASSIC);
	MappedFrame frame2 = mapping.GetFrame(2);
	MappedFrame frame3 = mapping.GetFrame(3);

	// Check for 3 fields of frame 2
	CHECK_EQUAL(2, frame2.Odd.Frame);
	CHECK_EQUAL(2, frame2.Even.Frame);
	CHECK_EQUAL(2, frame3.Odd.Frame);
	CHECK_EQUAL(3, frame3.Even.Frame);
}

TEST(FrameMapper_24_fps_to_30_fps_Pulldown_Advanced)
{
	// Create mapping 24 fps and 29.97 fps
	FrameMapper mapping(100, Framerate(24, 1), Framerate(30000, 1001), PULLDOWN_ADVANCED);
	MappedFrame frame2 = mapping.GetFrame(2);
	MappedFrame frame3 = mapping.GetFrame(3);
	MappedFrame frame4 = mapping.GetFrame(4);

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
	// Create mapping 24 fps and 29.97 fps
	FrameMapper mapping(100, Framerate(24, 1), Framerate(30000, 1001), PULLDOWN_NONE);
	MappedFrame frame4 = mapping.GetFrame(4);
	MappedFrame frame5 = mapping.GetFrame(5);

	// Check for advanced pulldown (only 1 fake frame)
	CHECK_EQUAL(4, frame4.Odd.Frame);
	CHECK_EQUAL(4, frame4.Even.Frame);
	CHECK_EQUAL(4, frame5.Odd.Frame);
	CHECK_EQUAL(4, frame5.Even.Frame);
}

TEST(FrameMapper_30_fps_to_24_fps_Pulldown_Classic)
{
	// Create mapping between 29.97 fps and 24 fps
	FrameMapper mapping(100, Framerate(30000, 1001), Framerate(24, 1), PULLDOWN_CLASSIC);
	MappedFrame frame3 = mapping.GetFrame(3);
	MappedFrame frame4 = mapping.GetFrame(4);
	MappedFrame frame5 = mapping.GetFrame(5);

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
	// Create mapping between 29.97 fps and 24 fps
	FrameMapper mapping(100, Framerate(30000, 1001), Framerate(24, 1), PULLDOWN_ADVANCED);
	MappedFrame frame2 = mapping.GetFrame(2);
	MappedFrame frame3 = mapping.GetFrame(3);
	MappedFrame frame4 = mapping.GetFrame(4);

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
	// Create mapping between 29.97 fps and 24 fps
	FrameMapper mapping(100, Framerate(30000, 1001), Framerate(24, 1), PULLDOWN_NONE);
	MappedFrame frame4 = mapping.GetFrame(4);
	MappedFrame frame5 = mapping.GetFrame(5);

	// Check for advanced pulldown (only 1 fake frame)
	CHECK_EQUAL(4, frame4.Odd.Frame);
	CHECK_EQUAL(4, frame4.Even.Frame);
	CHECK_EQUAL(6, frame5.Odd.Frame);
	CHECK_EQUAL(6, frame5.Even.Frame);
}

TEST(FrameMapper_MapTime)
{
	// Create mapping 24 fps and 24 fps
	FrameMapper mapping(100, Framerate(24, 1), Framerate(24, 1), PULLDOWN_NONE);

	// Create a Keyframe to re-map time (forward, reverse, and then forward again)
	Keyframe kf;
	kf.AddPoint(openshot::Point(Coordinate(1, 1), LINEAR));
	kf.AddPoint(openshot::Point(Coordinate(40, 40), LINEAR));
	kf.AddPoint(openshot::Point(Coordinate(60, 20), LINEAR));
	kf.AddPoint(openshot::Point(Coordinate(100, 100), LINEAR));

	// Re-map time
	mapping.MapTime(kf);

	// Check for OutOfBoundsFrames
	CHECK_THROW(mapping.GetFrame(0), OutOfBoundsFrame);
	CHECK_THROW(mapping.GetFrame(101), OutOfBoundsFrame);

	// Check to see if time remapping worked correctly
	CHECK_EQUAL(1, mapping.GetFrame(1).Odd.Frame);
	CHECK_EQUAL(2, mapping.GetFrame(2).Odd.Frame);
	CHECK_EQUAL(39, mapping.GetFrame(39).Odd.Frame);
	CHECK_EQUAL(40, mapping.GetFrame(40).Odd.Frame);
	CHECK_EQUAL(39, mapping.GetFrame(41).Odd.Frame);
	CHECK_EQUAL(38, mapping.GetFrame(42).Odd.Frame);
	CHECK_EQUAL(21, mapping.GetFrame(59).Odd.Frame);
	CHECK_EQUAL(20, mapping.GetFrame(60).Odd.Frame);
	CHECK_EQUAL(22, mapping.GetFrame(61).Odd.Frame);
	CHECK_EQUAL(24, mapping.GetFrame(62).Odd.Frame);
	CHECK_EQUAL(90, mapping.GetFrame(95).Odd.Frame);
	CHECK_EQUAL(100, mapping.GetFrame(100).Odd.Frame);

}
