/**
 * @file
 * @brief Source file for Fraction class
 * @author Jonathan Thomas <jonathan@openshot.org>
 *
 * @section LICENSE
 *
 * Copyright (c) 2008-2013 OpenShot Studios, LLC
 * (http://www.openshotstudios.com). This file is part of
 * OpenShot Library (http://www.openshot.org), an open-source project
 * dedicated to delivering high quality video editing and animation solutions
 * to the world.
 *
 * OpenShot Library is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * OpenShot Library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with OpenShot Library.  If not, see <http://www.gnu.org/licenses/>.
 */

#include "../include/Fraction.h"

using namespace openshot;

// Constructor
Fraction::Fraction() :
	num(1), den(1) {
}
Fraction::Fraction(int num, int den) :
	num(num), den(den) {
}

// Return this fraction as a float (i.e. 1/2 = 0.5)
float Fraction::ToFloat() {
	return float(num) / float(den);
}

// Return this fraction as a double (i.e. 1/2 = 0.5)
double Fraction::ToDouble() {
	return double(num) / double(den);
}

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
Fraction Fraction::Reciprocal()
{
	// flip the fraction
	return Fraction(den, num);
}
