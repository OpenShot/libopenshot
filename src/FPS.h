/**
 * @file
 * @brief Header file for Fraction class
 * @author Jonathan Thomas <jonathan@openshot.org>
 * @author FeRD (Frank Dana) <ferdnyc@gmail.com>
 *
 * @ref License
 */

// Copyright (c) 2008-2022 OpenShot Studios, LLC
//
// SPDX-License-Identifier: LGPL-3.0-or-later

#ifndef OPENSHOT_FPS_H
#define OPENSHOT_FPS_H

#include "Fraction.h"

namespace openshot {

/**
 * @brief An openshot::Fraction subclass with converting powers
 */
class FPS : public openshot::Fraction {
public:
    std::string type{"FPS"};

    FPS() = default;
    FPS(const FPS& other) = default;
    FPS(const openshot::Fraction& other) : Fraction(other) {}
    FPS(int a, int b) : Fraction(a, b) {}

    FPS Reciprocal() const {
        return FPS(den, num);
    };

    // Unit conversions

    /// Convert a relative time (seconds since start) to the nearest whole frame number.
    int64_t frame(double time) const;

    /// Convert a frame number (1-based) to a relative time
    /// in whole and partial seconds.
    double time(int64_t frame) const;

    /// Find a frame's first sample, for a given sample rate
    int64_t sample(int64_t frame, int sample_rate) const;

    // Multiplication and division
    using Fraction::operator*;
    using Fraction::operator/;

    /// Multiplication in the form (openshot::FPS * numeric_value)
    template<class numT>
    numT operator*(const numT& other) const {
        return static_cast<numT>(ToDouble() * other);
    }

    /// Division in the form (openshot::FPS / numeric_value)
    template<class numT>
    numT operator/(const numT& other) const {
        return static_cast<numT>(ToDouble() / other);
    }
};

/// Multiplication in the form (numeric_value * openshot::FPS)
template<class numT>
numT operator*(const numT& left, const openshot::FPS& right) {
    return static_cast<numT>(left * right.ToDouble());
}

/// Division in the form (numeric_value / openshot::FPS)
template<class numT>
numT operator/(const numT& left, const openshot::FPS& right) {
    return static_cast<numT>(left / right.ToDouble());
}

// // Stream output operator for openshot::FPS
// template<class charT, class traits>
// std::basic_ostream<charT, traits>&
// operator<<(std::basic_ostream<charT, traits>& o, const openshot::FPS& f) {
//     std::basic_ostringstream<charT, traits> s;
//     s.flags(o.flags());
//     s.imbue(o.getloc());
//     s.precision(o.precision());
//     s << "FPS(" << f.num << ", " << f.den << ")";
//     return o << s.str();
// }

}  // namespace openshot
#endif
