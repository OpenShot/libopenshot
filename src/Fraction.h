/**
 * @file
 * @brief Header file for Fraction class
 * @author Jonathan Thomas <jonathan@openshot.org>
 *
 * @ref License
 */

// Copyright (c) 2008-2019 OpenShot Studios, LLC
//
// SPDX-License-Identifier: LGPL-3.0-or-later

#ifndef OPENSHOT_FRACTION_H
#define OPENSHOT_FRACTION_H

#include <string>    // for std::string
#include <utility>   // for std::pair
#include <map>       // for std::map
#include <vector>    // for std::vector

namespace openshot {

/**
 * @brief This class represents a fraction
 *
 * Fractions are often used in video editing to represent ratios and rates, for example:
 * pixel ratios, frames per second, timebase, and other common ratios.  Fractions are preferred
 * over decimals due to their increased precision.
 */
class Fraction {
public:
	int num; ///<Numerator for the fraction
	int den; ///<Denominator for the fraction

	/// Default Constructor
	Fraction();

	/// Constructor with numerator and denominator
	Fraction(int num, int den);

	/// Constructor that accepts a (num, den) pair
	Fraction(std::pair<int, int> pair);

	/// Constructor that takes a vector of length 2 (containing {num, den})
	Fraction(std::vector<int> vector);

	/// Constructor that takes a key-value mapping (keys: 'num'. 'den')
	Fraction(std::map<std::string, int> mapping);

	/// Calculate the greatest common denominator
	int GreatestCommonDenominator();

	/// Reduce this fraction (i.e. 640/480 = 4/3)
	void Reduce();

	/// Return this fraction as a float (i.e. 1/2 = 0.5)
	float ToFloat();

	/// Return this fraction as a double (i.e. 1/2 = 0.5)
	double ToDouble() const;

	/// Return a rounded integer of the fraction (for example 30000/1001 returns 30)
	int ToInt();

	/// Return the reciprocal as a Fraction
	Fraction Reciprocal() const;

    // Multiplication and division

    /// Multiply two Fraction objects together
    openshot::Fraction operator*(openshot::Fraction other) {
        return openshot::Fraction(num * other.num, den * other.den);
    }

    /// Divide a Fraction by another Fraction
    openshot::Fraction operator/(openshot::Fraction other) {
        return *this * other.Reciprocal();
    }

    /// Multiplication in the form (openshot_Fraction * numeric_value)
    template<class numT>
    numT operator*(const numT& other) const {
        return static_cast<numT>(ToDouble() * other);
    }

    /// Division in the form (openshot_Fraction / numeric_value)
    template<class numT>
    numT operator/(const numT& other) const {
        return static_cast<numT>(ToDouble() / other);
    }
};

/// Multiplication in the form (numeric_value * openshot_Fraction)
template<class numT>
numT operator*(const numT& left, const openshot::Fraction& right) {
    return static_cast<numT>(left * right.ToDouble());
}

/// Division in the form (numeric_value / openshot_Fraction)
template<class numT>
numT operator/(const numT& left, const openshot::Fraction& right) {
    return static_cast<numT>(left / right.ToDouble());
}

// Stream output operator for openshot::Fraction
template<class charT, class traits>
std::basic_ostream<charT, traits>&
operator<<(std::basic_ostream<charT, traits>& o, const openshot::Fraction& frac) {
    std::basic_ostringstream<charT, traits> s;
    s.flags(o.flags());
    s.imbue(o.getloc());
    s.precision(o.precision());
    s << "Fraction(" << frac.num << ", " << frac.den << ")";
    return o << s.str();
}

}  // namespace openshot
#endif
