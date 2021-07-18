/**
 * @file
 * @brief Source file for Fraction class
 * @author Jonathan Thomas <jonathan@openshot.org>
 *
 * @ref License
 */

/* LICENSE
 *
 * Copyright (c) 2008-2019 OpenShot Studios, LLC
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

#include "Fraction.h"
#include <cmath>

using namespace openshot;

// Delegating constructors
Fraction::Fraction() : Fraction::Fraction(1, 1) {}

Fraction::Fraction(std::pair<int, int> pair)
	: Fraction::Fraction(pair.first, pair.second) {}

Fraction::Fraction(std::map<std::string, int> mapping)
	: Fraction::Fraction(mapping["num"], mapping["den"]) {}

Fraction::Fraction(std::vector<int> vector)
	: Fraction::Fraction(vector[0], vector[1]) {}

// Full constructor
Fraction::Fraction(int num, int den) :
	num(num), den(den) {
}

// Return this fraction as a float (i.e. 1/2 = 0.5)
float Fraction::ToFloat() {
	return float(num) / float(den);
}

// Return this fraction as a double (i.e. 1/2 = 0.5)
double Fraction::ToDouble() const {
	return double(num) / double(den);
}

// Return a rounded integer of the frame rate (for example 30000/1001 returns 30 fps)
int Fraction::ToInt() {
	return round((double) num / den);
}

// Calculate the greatest common denominator
int Fraction::GreatestCommonDenominator() {
	int first = num;
	int second = den;

	// Find the biggest whole number that will divide into both the numerator
	// and denominator
	int t;
	while (second != 0) {
		t = second;
		second = first % second;
		first = t;
	}
	return first;
}

void Fraction::Reduce() {
	// Get the greatest common denominator
	int GCD = GreatestCommonDenominator();

	// Reduce this fraction to the smallest possible whole numbers
	num = num / GCD;
	den = den / GCD;
}

// Return the reciprocal as a new Fraction
Fraction Fraction::Reciprocal() const
{
	// flip the fraction
	return Fraction(den, num);
}
