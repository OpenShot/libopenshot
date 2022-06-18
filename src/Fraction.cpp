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
