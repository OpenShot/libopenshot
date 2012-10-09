#include "UnitTest++.h"
#include "../include/OpenShot.h"

using namespace std;
using namespace openshot;

TEST(Clip_Default_Constructor)
{
	// Create a empty clip
	Clip c1;

	// Check basic settings
	CHECK_EQUAL(ANCHOR_CANVAS, c1.anchor);
	CHECK_EQUAL(GRAVITY_CENTER, c1.gravity);
	CHECK_EQUAL(SCALE_FIT, c1.scale);
	CHECK_EQUAL(0, c1.Layer());
	CHECK_CLOSE(0.0f, c1.Position(), 0.00001);
	CHECK_CLOSE(0.0f, c1.Start(), 0.00001);
	CHECK_CLOSE(0.0f, c1.End(), 0.00001);
}

TEST(Clip_Constructor)
{
	// Create a empty clip
	Clip c1("../../src/examples/piano.wav");
	c1.Open();

	// Check basic settings
	CHECK_EQUAL(ANCHOR_CANVAS, c1.anchor);
	CHECK_EQUAL(GRAVITY_CENTER, c1.gravity);
	CHECK_EQUAL(SCALE_FIT, c1.scale);
	CHECK_EQUAL(0, c1.Layer());
	CHECK_CLOSE(0.0f, c1.Position(), 0.00001);
	CHECK_CLOSE(0.0f, c1.Start(), 0.00001);
	CHECK_CLOSE(4.39937f, c1.End(), 0.00001);
}

TEST(Clip_Basic_Gettings_and_Setters)
{
	// Create a empty clip
	Clip c1;
	c1.Open();

	// Check basic settings
	CHECK_EQUAL(ANCHOR_CANVAS, c1.anchor);
	CHECK_EQUAL(GRAVITY_CENTER, c1.gravity);
	CHECK_EQUAL(SCALE_FIT, c1.scale);
	CHECK_EQUAL(0, c1.Layer());
	CHECK_CLOSE(0.0f, c1.Position(), 0.00001);
	CHECK_CLOSE(0.0f, c1.Start(), 0.00001);
	CHECK_CLOSE(0.0f, c1.End(), 0.00001);

	// Change some properties
	c1.Layer(1);
	c1.Position(5.0);
	c1.Start(3.5);
	c1.End(10.5);

	CHECK_EQUAL(1, c1.Layer());
	CHECK_CLOSE(5.0f, c1.Position(), 0.00001);
	CHECK_CLOSE(3.5f, c1.Start(), 0.00001);
	CHECK_CLOSE(10.5f, c1.End(), 0.00001);
}
