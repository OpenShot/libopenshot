// SPDX-FileCopyrightText: 2026 OpenShot Studios, LLC
// SPDX-License-Identifier: LGPL-3.0-or-later

#include "ColorSpace.h"
#include <stdexcept>
#include <cmath>
#include <algorithm>

namespace openshot {
namespace {

struct Matrix3 {
    double m[3][3];
};

Matrix3 RGBToXYZ(RGBColorSpace space) {
    switch (space) {
    case RGBColorSpace::BT709:
        return {{
            {0.4123907993, 0.3575843394, 0.1804807884},
            {0.2126390059, 0.7151686788, 0.0721923154},
            {0.0193308187, 0.1191947798, 0.9505321522}
        }};
    case RGBColorSpace::BT2020:
        return {{
            {0.6369580483, 0.1446169036, 0.1688809752},
            {0.2627002120, 0.6779980715, 0.0593017165},
            {0.0000000000, 0.0280726930, 1.0609850577}
        }};
    case RGBColorSpace::DisplayP3:
        return {{
            {0.4865709486, 0.2656676932, 0.1982172852},
            {0.2289745641, 0.6917385218, 0.0792869141},
            {0.0000000000, 0.0451133819, 1.0439443689}
        }};
    }
    throw std::invalid_argument("Unsupported RGB color space");
}

LinearRGB Multiply(const Matrix3& m, const LinearRGB& v) {
    return {
        m.m[0][0]*v.r + m.m[0][1]*v.g + m.m[0][2]*v.b,
        m.m[1][0]*v.r + m.m[1][1]*v.g + m.m[1][2]*v.b,
        m.m[2][0]*v.r + m.m[2][1]*v.g + m.m[2][2]*v.b
    };
}

Matrix3 Inverse(const Matrix3& a) {
    const auto& m = a.m;

    const double det =
        m[0][0]*(m[1][1]*m[2][2]-m[1][2]*m[2][1])
      - m[0][1]*(m[1][0]*m[2][2]-m[1][2]*m[2][0])
      + m[0][2]*(m[1][0]*m[2][1]-m[1][1]*m[2][0]);

    if (det == 0.0)
        throw std::runtime_error("Singular color matrix");

    const double d = 1.0 / det;

    return {{
        {(m[1][1]*m[2][2]-m[1][2]*m[2][1])*d,
         (m[0][2]*m[2][1]-m[0][1]*m[2][2])*d,
         (m[0][1]*m[1][2]-m[0][2]*m[1][1])*d},

        {(m[1][2]*m[2][0]-m[1][0]*m[2][2])*d,
         (m[0][0]*m[2][2]-m[0][2]*m[2][0])*d,
         (m[0][2]*m[1][0]-m[0][0]*m[1][2])*d},

        {(m[1][0]*m[2][1]-m[1][1]*m[2][0])*d,
         (m[0][1]*m[2][0]-m[0][0]*m[2][1])*d,
         (m[0][0]*m[1][1]-m[0][1]*m[1][0])*d}
    }};
}

} // namespace

LinearRGB ConvertLinearRGB(
    const LinearRGB& rgb,
    RGBColorSpace source,
    RGBColorSpace destination)
{
    const Matrix3 src = RGBToXYZ(source);
    const Matrix3 dst = RGBToXYZ(destination);

    if (source == destination)
        return rgb;

    const LinearRGB xyz = Multiply(src, rgb);
    return Multiply(Inverse(dst), xyz);
}


double DecodeTransfer(double encoded, TransferFunction function)
{
    if (!std::isfinite(encoded) || encoded < 0.0 || encoded > 1.0)
        throw std::out_of_range("Encoded value outside [0, 1]");

    switch (function) {
    case TransferFunction::BT709:
        return encoded < 0.081
            ? encoded / 4.5
            : std::pow((encoded + 0.099) / 1.099, 1.0 / 0.45);

    case TransferFunction::SRGB:
        return encoded <= 0.04045
            ? encoded / 12.92
            : std::pow((encoded + 0.055) / 1.055, 2.4);

    case TransferFunction::PQ: {
        constexpr double m1 = 2610.0 / 16384.0;
        constexpr double m2 = 2523.0 / 32.0;
        constexpr double c1 = 3424.0 / 4096.0;
        constexpr double c2 = 2413.0 / 128.0;
        constexpr double c3 = 2392.0 / 128.0;

        const double n = std::pow(encoded, 1.0 / m2);
        const double numerator = std::max(n - c1, 0.0);
        return 10000.0 * std::pow(
            numerator / (c2 - c3 * n), 1.0 / m1);
    }

    case TransferFunction::HLG: {
        constexpr double a = 0.17883277;
        constexpr double b = 1.0 - 4.0 * a;
        constexpr double c = 0.55991073;

        return encoded <= 0.5
            ? (encoded * encoded) / 3.0
            : (std::exp((encoded - c) / a) + b) / 12.0;
    }

    default:
        throw std::invalid_argument("Transfer function not implemented");
    }
}


double EncodeTransfer(double linear, TransferFunction function)
{
    if (!std::isfinite(linear) || linear < 0.0)
        throw std::out_of_range("Linear value must be finite and nonnegative");

    switch (function) {
    case TransferFunction::BT709:
        if (linear > 1.0)
            throw std::out_of_range("SDR linear value exceeds 1");
        return linear < 0.018
            ? 4.5 * linear
            : 1.099 * std::pow(linear, 0.45) - 0.099;

    case TransferFunction::SRGB:
        if (linear > 1.0)
            throw std::out_of_range("SDR linear value exceeds 1");
        return linear <= 0.0031308
            ? 12.92 * linear
            : 1.055 * std::pow(linear, 1.0 / 2.4) - 0.055;

    case TransferFunction::PQ: {
        if (linear > 10000.0)
            throw std::out_of_range("PQ luminance exceeds 10000 nits");

        constexpr double m1 = 2610.0 / 16384.0;
        constexpr double m2 = 2523.0 / 32.0;
        constexpr double c1 = 3424.0 / 4096.0;
        constexpr double c2 = 2413.0 / 128.0;
        constexpr double c3 = 2392.0 / 128.0;

        const double n = std::pow(linear / 10000.0, m1);
        return std::pow(
            (c1 + c2 * n) / (1.0 + c3 * n), m2);
    }

    case TransferFunction::HLG: {
        if (linear > 1.0)
            throw std::out_of_range("HLG linear value exceeds 1");

        constexpr double a = 0.17883277;
        constexpr double b = 1.0 - 4.0 * a;
        constexpr double c = 0.55991073;

        return linear <= (1.0 / 12.0)
            ? std::sqrt(3.0 * linear)
            : a * std::log(12.0 * linear - b) + c;
    }

    default:
        throw std::invalid_argument("Transfer function not implemented");
    }
}

} // namespace openshot
