#include "ColorSpace.h"
#include "openshot_catch.h"
#include <cmath>

using namespace openshot;

TEST_CASE("ColorSpace identity", "[ColorSpace]") {
    const LinearRGB sample{0.25, 0.5, 0.75};

    for (auto space : {RGBColorSpace::BT709,
                       RGBColorSpace::BT2020,
                       RGBColorSpace::DisplayP3}) {
        auto result = ConvertLinearRGB(sample, space, space);
        REQUIRE(result.r == sample.r);
        REQUIRE(result.g == sample.g);
        REQUIRE(result.b == sample.b);
    }
}

TEST_CASE("ColorSpace white point", "[ColorSpace]") {
    const LinearRGB white{1.0, 1.0, 1.0};

    for (auto src : {RGBColorSpace::BT709,
                     RGBColorSpace::BT2020,
                     RGBColorSpace::DisplayP3}) {
        for (auto dst : {RGBColorSpace::BT709,
                         RGBColorSpace::BT2020,
                         RGBColorSpace::DisplayP3}) {
            auto result = ConvertLinearRGB(white, src, dst);
            REQUIRE(std::abs(result.r - 1.0) < 1e-8);
            REQUIRE(std::abs(result.g - 1.0) < 1e-8);
            REQUIRE(std::abs(result.b - 1.0) < 1e-8);
        }
    }
}

TEST_CASE("ColorSpace round trip", "[ColorSpace]") {
    const LinearRGB sample{0.3, 0.5, 0.7};

    for (auto src : {RGBColorSpace::BT709,
                     RGBColorSpace::BT2020,
                     RGBColorSpace::DisplayP3}) {
        for (auto dst : {RGBColorSpace::BT709,
                         RGBColorSpace::BT2020,
                         RGBColorSpace::DisplayP3}) {
            auto forward = ConvertLinearRGB(sample, src, dst);
            auto reverse = ConvertLinearRGB(forward, dst, src);

            REQUIRE(std::abs(reverse.r - sample.r) < 1e-9);
            REQUIRE(std::abs(reverse.g - sample.g) < 1e-9);
            REQUIRE(std::abs(reverse.b - sample.b) < 1e-9);
        }
    }
}

TEST_CASE("BT709 red to BT2020", "[ColorSpace]") {
    const LinearRGB red{1.0, 0.0, 0.0};

    auto result = ConvertLinearRGB(
        red, RGBColorSpace::BT709, RGBColorSpace::BT2020);

    REQUIRE(std::abs(result.r - 0.6274) < 0.001);
    REQUIRE(std::abs(result.g - 0.0691) < 0.001);
    REQUIRE(std::abs(result.b - 0.0164) < 0.001);
}
