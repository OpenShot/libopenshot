/**
 * @file
 * @brief Unit tests for openshot::Fraction
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
