/**
 * @file
 * @brief Source file for Fraction class
 * @author Jonathan Thomas <jonathan@openshot.org>
 *
 * @ref License
 */

// Copyright (c) 2008-2019 OpenShot Studios, LLC
//
// SPDX-License-Identifier: LGPL-3.0-or-later

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
	num(num), den(den) {}

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
	if (GCD == 0) {
		return;
	}

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
