#include "UnitTest++.h"
#include "../include/OpenShot.h"

using namespace std;
using namespace openshot;

TEST(FrameRate_Check_Rounded_24_FPS)
{
	// Create framerate for 24 fps
	Framerate rate(24, 1);

	CHECK_EQUAL(24, rate.GetRoundedFPS());
}

TEST(FrameRate_Check_Rounded_25_FPS)
{
	// Create framerate for 25 fps
	Framerate rate(25, 1);

	CHECK_EQUAL(25, rate.GetRoundedFPS());
}

TEST(FrameRate_Check_Rounded_29_97_FPS)
{
	// Create framerate for 29.97 fps
	Framerate rate(30000, 1001);

	CHECK_EQUAL(30, rate.GetRoundedFPS());
}

TEST(FrameRate_Check_Decimal_24_FPS)
{
	// Create framerate for 24 fps
	Framerate rate(24, 1);

	CHECK_CLOSE(24.0f, rate.GetFPS(), 0.0001);
}

TEST(FrameRate_Check_Decimal_25_FPS)
{
	// Create framerate for 24 fps
	Framerate rate(25, 1);

	CHECK_CLOSE(25.0f, rate.GetFPS(), 0.0001);
}

TEST(FrameRate_Check_Decimal_29_97_FPS)
{
	// Create framerate for 29.97 fps
	Framerate rate(30000, 1001);

	CHECK_CLOSE(29.97f, rate.GetFPS(), 0.0001);
}

