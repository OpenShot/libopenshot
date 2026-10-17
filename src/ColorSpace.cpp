// SPDX-FileCopyrightText: 2026 OpenShot Studios, LLC
// SPDX-License-Identifier: LGPL-3.0-or-later

#include "ColorSpace.h"
#include <stdexcept>

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

} // namespace openshot
