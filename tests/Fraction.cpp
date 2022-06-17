/**
 * @file
 * @brief Unit tests for openshot::Fraction
 * @author Jonathan Thomas <jonathan@openshot.org>
 *
 * @ref License
 */

// Copyright (c) 2008-2019 OpenShot Studios, LLC
//
// SPDX-License-Identifier: LGPL-3.0-or-later

#include "openshot_catch.h"

#include <map>
#include <vector>
#include <sstream>

#include "Fraction.h"

using namespace openshot;

TEST_CASE( "Constructors", "[libopenshot][fraction]" )
{
	// Create a default fraction (should be 1/1)
	Fraction f1;

	// Check default fraction
	CHECK(f1.num == 1);
	CHECK(f1.den == 1);
	CHECK(f1.ToFloat() == Approx(1.0f).margin(0.00001));
	CHECK(f1.ToDouble() == Approx(1.0f).margin(0.00001));

	// reduce fraction
	f1.Reduce();

	// Check the reduced fraction
	CHECK(f1.num == 1);
	CHECK(f1.den == 1);
	CHECK(f1.ToFloat() == Approx(1.0f).margin(0.00001));
	CHECK(f1.ToDouble() == Approx(1.0f).margin(0.00001));
}

TEST_CASE( "Alt_Constructors", "[libopenshot][fraction]" )
{
	// Use the delegating constructor for std::pair
	std::pair<int, int> args{24, 1};
	Fraction f1(args);
	CHECK(f1.num == 24);
	CHECK(f1.den == 1);
	CHECK(f1.ToFloat() == Approx(24.0f).margin(0.00001));

	// Use the delegating constructor for std::vector
	std::vector<int> v{30000, 1001};
	Fraction f2(v);
	CHECK(f2.ToFloat() == Approx(30000.0/1001.0).margin(0.00001));

	// Use the delegating constructor for std::map<std::string, int>
	std::map<std::string, int> dict;
	dict.insert({"num", 24000});
	dict.insert({"den", 1001});
	Fraction f3(dict);
	CHECK(f3.den == 1001);
	CHECK(f3.num == 24000);
	CHECK(f3.Reciprocal().ToFloat() == Approx(1001.0/24000.0).margin(0.00001));
}

TEST_CASE( "WxH_640_480", "[libopenshot][fraction]" )
{
	// Create fraction
	Fraction f1(640, 480);

	// Check fraction
	CHECK(f1.num == 640);
	CHECK(f1.den == 480);
	CHECK(f1.ToFloat() == Approx(1.33333f).margin(0.00001));
	CHECK(f1.ToDouble() == Approx(1.33333f).margin(0.00001));

	// reduce fraction
	f1.Reduce();

	// Check the reduced fraction
	CHECK(f1.num == 4);
	CHECK(f1.den == 3);
	CHECK(f1.ToFloat() == Approx(1.33333f).margin(0.00001));
	CHECK(f1.ToDouble() == Approx(1.33333f).margin(0.00001));
}

TEST_CASE( "WxH_1280_720", "[libopenshot][fraction]" )
{
	// Create fraction
	Fraction f1(1280, 720);

	// Check fraction
	CHECK(f1.num == 1280);
	CHECK(f1.den == 720);
	CHECK(f1.ToFloat() == Approx(1.77777f).margin(0.00001));
	CHECK(f1.ToDouble() == Approx(1.77777f).margin(0.00001));

	// reduce fraction
	f1.Reduce();

	// Check the reduced fraction
	CHECK(f1.num == 16);
	CHECK(f1.den == 9);
	CHECK(f1.ToFloat() == Approx(1.77777f).margin(0.00001));
	CHECK(f1.ToDouble() == Approx(1.77777f).margin(0.00001));
}

TEST_CASE( "Reciprocal", "[libopenshot][fraction]" )
{
	// Create fraction
	Fraction f1(1280, 720);

	// Check fraction
	CHECK(f1.num == 1280);
	CHECK(f1.den == 720);
	CHECK(f1.ToFloat() == Approx(1.77777f).margin(0.00001));
	CHECK(f1.ToDouble() == Approx(1.77777f).margin(0.00001));

	// Get the reciprocal of the fraction (i.e. flip the fraction)
	Fraction f2 = f1.Reciprocal();

	// Check the reduced fraction
	CHECK(f2.num == 720);
	CHECK(f2.den == 1280);
	CHECK(f2.ToFloat() == Approx(0.5625f).margin(0.00001));
	CHECK(f2.ToDouble() == Approx(0.5625f).margin(0.00001));

	// Re-Check the original fraction (to be sure it hasn't changed)
	CHECK(f1.num == 1280);
	CHECK(f1.den == 720);
	CHECK(f1.ToFloat() == Approx(1.77777f).margin(0.00001));
	CHECK(f1.ToDouble() == Approx(1.77777f).margin(0.00001));
}

TEST_CASE( "Fraction operations", "[libopenshot][fraction]" ) {
    openshot::Fraction f1(30, 1);
    openshot::Fraction f2(3, 9);

    // Multiply two Fractions
    auto f3 = f1 * f2;
    CHECK(f3.num == 90);
    CHECK(f3.den == 9);

    // Divide a Fraction by a Fraction
    auto f4 = f1 / f2;
    CHECK(f4.num == 270);
    CHECK(f4.den == 3);
}

TEST_CASE( "Numeric multiplication", "[libopenshot][fraction]" )
{
    openshot::Fraction f1(30000, 1001);
    const int64_t num1 = 12;
    const double num2 = 13.6;
    const float num3 = 14.1;
    const int num4 = 15;

    // operator* with Fraction on LHS
    CHECK(f1 * num1 == static_cast<int64_t>(f1.ToDouble() * num1));
    CHECK_FALSE(f1 * num1 == f1.ToDouble() * num1);
    CHECK_FALSE(f1 * num1 == f1.ToInt() * num1);

    CHECK(f1 * num2 == Approx(static_cast<double>(f1.ToDouble() * num2))
                       .margin(0.0001));
    CHECK(f1 * num3 == Approx(static_cast<float>(f1.ToDouble() * num3))
                       .margin(0.0001));

    CHECK(f1 * num4 == static_cast<int>(f1.ToDouble() * num4));
    CHECK_FALSE(f1 * num4 == f1.ToDouble() * num4);
    CHECK_FALSE(f1 * num4 == f1.ToInt() * num4);

    // operator* with Fraction on RHS
    CHECK(num1 * f1 == static_cast<int64_t>(f1.ToDouble() * num1));
    CHECK_FALSE(num1 * f1 == num1 * f1.ToDouble());
    CHECK_FALSE(num1 * f1 == num1 * f1.ToInt());

    CHECK(num2 * f1 == Approx(static_cast<double>(f1.ToDouble() * num2))
                       .margin(0.0001));
    CHECK(num3 * f1 == Approx(static_cast<float>(f1.ToDouble() * num3))
                       .margin(0.0001));

    CHECK(num4 * f1 == static_cast<int>(f1.ToDouble() * num4));
    CHECK_FALSE(num4 * f1 == num4 * f1.ToDouble());
    CHECK_FALSE(num4 * f1 == num4 * f1.ToInt());

    // Transposition
    CHECK(num1 * f1 == f1 * num1);
    CHECK(num2 * f1 == Approx(f1 * num2).margin(0.0001));
    CHECK(num3 * f1 == Approx(f1 * num3).margin(0.0001));
    CHECK(num4 * f1 == f1 * num4);
}

TEST_CASE( "Numeric division", "[libopenshot][fraction]" )
{
    openshot::Fraction f1(24000, 1001);
    openshot::Fraction f2(1001, 30000);
    const int64_t num1 = 2;
    const double num2 = 3.5;
    const float num3 = 4.99;
    const int num4 = 5;


    // operator* with Fraction on LHS
    CHECK(f1 / num1 == static_cast<int64_t>(f1.ToDouble() / num1));
    CHECK(f1 / num2 == Approx(static_cast<double>(f1.ToDouble() / num2))
                       .margin(0.0001));
    CHECK(f1 / num3 == Approx(static_cast<float>(f1.ToDouble() / num3))
                       .margin(0.0001));
    CHECK(f1 / num4 == static_cast<int>(f1.ToDouble() / num4));

    CHECK(f2 / num1 == static_cast<int64_t>(f2.ToDouble() / num1));
    CHECK(f2 / num2 == Approx(static_cast<double>(f2.ToDouble() / num2))
                       .margin(0.0001));
    CHECK(f2 / num3 == Approx(static_cast<float>(f2.ToDouble() / num3))
                       .margin(0.0001));
    CHECK(f2 / num4 == static_cast<int>(f2.ToDouble() / num4));

    // operator* with Fraction on RHS
    CHECK(num1 / f1 == static_cast<int64_t>(num1 / f1.ToDouble()));
    CHECK(num2 / f1 == Approx(static_cast<double>(num2 / f1.ToDouble()))
                       .margin(0.0001));
    CHECK(num3 / f1 == Approx(static_cast<float>(num3 / f1.ToDouble()))
                       .margin(0.0001));
    CHECK(num4 / f1 == static_cast<int>(num4 / f1.ToDouble()));

    CHECK(num1 / f2 == static_cast<int64_t>(num1 / f2.ToDouble()));
    CHECK(num2 / f2 == Approx(static_cast<double>(num2 / f2.ToDouble()))
                       .margin(0.0001));
    CHECK(num3 / f2 == Approx(static_cast<float>(num3 / f2.ToDouble()))
                       .margin(0.0001));
    CHECK(num4 / f2 == static_cast<int>(num4 / f2.ToDouble()));
}

TEST_CASE( "Operator ostream", "[libopenshot][fraction]" )
{
	std::stringstream output;
	openshot::Fraction f3(30000, 1001);

	output << f3;
	CHECK(output.str() == "Fraction(30000, 1001)");
}
