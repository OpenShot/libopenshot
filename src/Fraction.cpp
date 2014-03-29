/**
 * @file
 * @brief Source file for Fraction class
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
 * and/or modify it under the terms of the GNU Affero General Public License
 * as published by the Free Software Foundation, either version 3 of the
 * License, or (at your option) any later version.
 *
 * OpenShot Library (libopenshot) is distributed in the hope that it will be
 * useful, but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU Affero General Public License for more details.
 *
 * You should have received a copy of the GNU Affero General Public License
 * along with OpenShot Library. If not, see <http://www.gnu.org/licenses/>.
 *
 * Also, if your software can interact with users remotely through a computer
 * network, you should also make sure that it provides a way for users to
 * get its source. For example, if your program is a web application, its
 * interface could display a "Source" link that leads users to an archive
 * of the code. There are many ways you could offer source, and different
 * solutions will be better for different programs; see section 13 for the
 * specific requirements.
 *
 * You should also get your employer (if you work as a programmer) or school,
 * if any, to sign a "copyright disclaimer" for the program, if necessary.
 * For more information on this, and how to apply and follow the GNU AGPL, see
 * <http://www.gnu.org/licenses/>.
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

// Return a rounded integer of the frame rate (for example 30000/1001 returns 30 fps)
int Fraction::ToInt() {
	return round((float) num / den);
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
Fraction Fraction::Reciprocal()
{
	// flip the fraction
	return Fraction(den, num);
}
