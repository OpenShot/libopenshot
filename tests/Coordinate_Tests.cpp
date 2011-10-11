#include "UnitTest++.h"
#include "../include/OpenShot.h"

using namespace std;
using namespace openshot;

TEST(Coordinate_Default_Constructor)
{
	// Create an empty coordinate
	Coordinate c1;

	CHECK_CLOSE(0.0f, c1.X, 0.00001);
	CHECK_CLOSE(0.0f, c1.Y, 0.00001);
}

TEST(Coordinate_X_Y_Constructor)
{
	// Create an empty coordinate
	Coordinate c1(2,8);

	CHECK_CLOSE(2.0f, c1.X, 0.00001);
	CHECK_CLOSE(8.0f, c1.Y, 0.00001);
}
