#include "UnitTest++.h"
#include "../include/OpenShot.h"

using namespace std;
using namespace openshot;

TEST(Fraction_Default_Constructor)
{
	// Create a default fraction (should be 1/1)
	Fraction f1;

	// Check default fraction
	CHECK_EQUAL(1, f1.num);
	CHECK_EQUAL(1, f1.den);
	CHECK_CLOSE(1.0f, f1.ToFloat(), 0.00001);
	CHECK_CLOSE(1.0f, f1.ToDouble(), 0.00001);

	// reduce fraction
	f1.Reduce();

	// Check the reduced fraction
	CHECK_EQUAL(1, f1.num);
	CHECK_EQUAL(1, f1.den);
	CHECK_CLOSE(1.0f, f1.ToFloat(), 0.00001);
	CHECK_CLOSE(1.0f, f1.ToDouble(), 0.00001);
}

TEST(Fraction_640_480)
{
	// Create fraction
	Fraction f1(640, 480);

	// Check fraction
	CHECK_EQUAL(640, f1.num);
	CHECK_EQUAL(480, f1.den);
	CHECK_CLOSE(1.33333f, f1.ToFloat(), 0.00001);
	CHECK_CLOSE(1.33333f, f1.ToDouble(), 0.00001);

	// reduce fraction
	f1.Reduce();

	// Check the reduced fraction
	CHECK_EQUAL(4, f1.num);
	CHECK_EQUAL(3, f1.den);
	CHECK_CLOSE(1.33333f, f1.ToFloat(), 0.00001);
	CHECK_CLOSE(1.33333f, f1.ToDouble(), 0.00001);
}

TEST(Fraction_1280_720)
{
	// Create fraction
	Fraction f1(1280, 720);

	// Check fraction
	CHECK_EQUAL(1280, f1.num);
	CHECK_EQUAL(720, f1.den);
	CHECK_CLOSE(1.77777f, f1.ToFloat(), 0.00001);
	CHECK_CLOSE(1.77777f, f1.ToDouble(), 0.00001);

	// reduce fraction
	f1.Reduce();

	// Check the reduced fraction
	CHECK_EQUAL(16, f1.num);
	CHECK_EQUAL(9, f1.den);
	CHECK_CLOSE(1.77777f, f1.ToFloat(), 0.00001);
	CHECK_CLOSE(1.77777f, f1.ToDouble(), 0.00001);
}

TEST(Fraction_reciprocal)
{
	// Create fraction
	Fraction f1(1280, 720);

	// Check fraction
	CHECK_EQUAL(1280, f1.num);
	CHECK_EQUAL(720, f1.den);
	CHECK_CLOSE(1.77777f, f1.ToFloat(), 0.00001);
	CHECK_CLOSE(1.77777f, f1.ToDouble(), 0.00001);

	// Get the reciprocal of the fraction (i.e. flip the fraction)
	Fraction f2 = f1.Reciprocal();

	// Check the reduced fraction
	CHECK_EQUAL(720, f2.num);
	CHECK_EQUAL(1280, f2.den);
	CHECK_CLOSE(0.5625f, f2.ToFloat(), 0.00001);
	CHECK_CLOSE(0.5625f, f2.ToDouble(), 0.00001);

	// Re-Check the original fraction (to be sure it hasn't changed)
	CHECK_EQUAL(1280, f1.num);
	CHECK_EQUAL(720, f1.den);
	CHECK_CLOSE(1.77777f, f1.ToFloat(), 0.00001);
	CHECK_CLOSE(1.77777f, f1.ToDouble(), 0.00001);
}
