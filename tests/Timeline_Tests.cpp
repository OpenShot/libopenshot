#include "UnitTest++.h"
#include "../include/OpenShot.h"

using namespace std;
using namespace openshot;

TEST(Timeline_Constructor)
{
	// Create a default fraction (should be 1/1)
	Framerate fps(30000,1000);
	Timeline t1(640, 480, fps, 44100, 2);

	// Check values
	CHECK_EQUAL(640, t1.Width());
	CHECK_EQUAL(480, t1.Height());

	// Create a default fraction (should be 1/1)
	Timeline t2(300, 240, fps, 44100, 2);

	// Check values
	CHECK_EQUAL(300, t2.Width());
	CHECK_EQUAL(240, t2.Height());
}

TEST(Timeline_Width_and_Height_Functions)
{
	// Create a default fraction (should be 1/1)
	Framerate fps(30000,1000);
	Timeline t1(640, 480, fps, 44100, 2);

	// Check values
	CHECK_EQUAL(640, t1.Width());
	CHECK_EQUAL(480, t1.Height());

	// Set width
	t1.Width(600);

	// Check values
	CHECK_EQUAL(600, t1.Width());
	CHECK_EQUAL(480, t1.Height());

	// Set height
	t1.Height(400);

	// Check values
	CHECK_EQUAL(600, t1.Width());
	CHECK_EQUAL(400, t1.Height());
}

TEST(Timeline_Framerate)
{
	// Create a default fraction (should be 1/1)
	Framerate fps(24,1);
	Timeline t1(640, 480, fps, 44100, 2);

	// Check values
	CHECK_CLOSE(24.0f, t1.FrameRate().GetFPS(), 0.00001);

	// Set width
	t1.FrameRate(Framerate(30000,1001));

	// Check values
	CHECK_CLOSE(29.97002f, t1.FrameRate().GetFPS(), 0.00001);

}
