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
#include <cmath>     // for std::round

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
    int num{1};  ///< Numerator for the fraction
    int den{1};  ///< Denominator for the fraction
    std::string type{"Fraction"};  ///< Type name for the class (overridable)

    /// Default Constructor
    Fraction() = default;

    // Copy constructor
    Fraction(const Fraction&) = default;

    /// Constructor with numerator and denominator
    Fraction(int num, int den) : num(num), den(den) {}

    /// Constructor that accepts a (num, den) pair
    Fraction(std::pair<int, int> pair) : num(pair.first), den(pair.second) {}

    /// Constructor that takes a vector of length 2 (containing {num, den})
    Fraction(std::vector<int> vector)
        : Fraction::Fraction(vector[0], vector[1]) {}

    /// Constructor that takes a key-value mapping (keys: 'num'. 'den')
    Fraction(std::map<std::string, int> mapping)
        : Fraction::Fraction(mapping["num"], mapping["den"]) {}

    /// Calculate the greatest common denominator
    virtual int GreatestCommonDenominator();

    /// Reduce this fraction (i.e. 640/480 = 4/3)
    virtual void Reduce();

    /// Return this fraction as a float (i.e. 1/2 = 0.5)
    virtual float ToFloat() {
        return float(num) / float(den);
    }

    /// Return this fraction as a double (i.e. 1/2 = 0.5)
    virtual double ToDouble() const {
        return double(num) / double(den);
    }

    /// Return a rounded integer of the fraction (for example 30000/1001 returns 30)
    virtual int ToInt() {
        return std::round((double) num / den);
    }

    /// Return the reciprocal as a Fraction
    Fraction Reciprocal() const {
        return Fraction(den, num);
    };

    // Multiplication and division

    /// Multiply two Fraction objects together
    Fraction operator*(Fraction other) {
        return Fraction(num * other.num, den * other.den);
    }

    /// Divide a Fraction by another Fraction
    Fraction operator/(Fraction other) {
        return *this * other.Reciprocal();
    }

    /// Multiplication in the form (openshot_Fraction * numeric_value)
    template<class numT>
    numT operator*(const numT& other) const {
        return static_cast<numT>(this->ToDouble() * other);
    }

    /// Division in the form (openshot_Fraction / numeric_value)
    template<class numT>
    numT operator/(const numT& other) const {
        return static_cast<numT>(this->ToDouble() / other);
    }

    /// Destructor for virtual dispatch (defaulted)
    virtual ~Fraction() = default;
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
    s << frac.type << '(' << frac.num << ", " << frac.den << ')';
    return o << s.str();
}

}  // namespace openshot
#endif
