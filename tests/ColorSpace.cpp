#include "ColorSpace.h"
#include "openshot_catch.h"
#include <cmath>
#include <limits>

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

TEST_CASE("SDR transfer reference values", "[ColorSpace][Transfer]") {
    using namespace openshot;

    CHECK(DecodeTransfer(0.0, TransferFunction::BT709) == Approx(0.0));
    CHECK(DecodeTransfer(1.0, TransferFunction::BT709) == Approx(1.0));
    CHECK(EncodeTransfer(0.018, TransferFunction::BT709) == Approx(0.081247944).epsilon(0.00001));

    CHECK(DecodeTransfer(0.04045, TransferFunction::SRGB) == Approx(0.003130805).epsilon(0.00001));
    CHECK(EncodeTransfer(0.5, TransferFunction::SRGB) == Approx(0.735356983).epsilon(0.00001));

    for (double x : {0.0, 0.01, 0.1, 0.5, 1.0}) {
        CHECK(DecodeTransfer(EncodeTransfer(x, TransferFunction::BT709),
                             TransferFunction::BT709) == Approx(x).margin(0.000001));
        CHECK(DecodeTransfer(EncodeTransfer(x, TransferFunction::SRGB),
                             TransferFunction::SRGB) == Approx(x).margin(0.000001));
    }
}

TEST_CASE("PQ transfer reference values", "[ColorSpace][PQ]") {
    using namespace openshot;

    CHECK(EncodeTransfer(100.0, TransferFunction::PQ)
          == Approx(0.508078).margin(0.000001));

    CHECK(EncodeTransfer(1000.0, TransferFunction::PQ)
          == Approx(0.751827).margin(0.000001));

    CHECK(EncodeTransfer(10000.0, TransferFunction::PQ)
          == Approx(1.0).margin(0.000001));

    CHECK(DecodeTransfer(0.0, TransferFunction::PQ)
          == Approx(0.0).margin(0.000001));

    for (double nits : {1.0, 100.0, 1000.0, 10000.0}) {
        double encoded = EncodeTransfer(nits, TransferFunction::PQ);
        CHECK(DecodeTransfer(encoded, TransferFunction::PQ)
              == Approx(nits).epsilon(0.000001));
    }

    CHECK_THROWS_AS(EncodeTransfer(10001.0, TransferFunction::PQ),
                    std::out_of_range);
    CHECK_THROWS_AS(DecodeTransfer(1.1, TransferFunction::PQ),
                    std::out_of_range);
}

TEST_CASE("HLG transfer reference values", "[ColorSpace][HLG]") {
    using namespace openshot;

    CHECK(DecodeTransfer(0.0, TransferFunction::HLG)
          == Approx(0.0).margin(1e-12));

    CHECK(DecodeTransfer(0.5, TransferFunction::HLG)
          == Approx(1.0 / 12.0).epsilon(1e-6));

    CHECK(DecodeTransfer(1.0, TransferFunction::HLG)
          == Approx(1.0).epsilon(1e-6));

    CHECK(EncodeTransfer(0.0, TransferFunction::HLG)
          == Approx(0.0).margin(1e-12));

    CHECK(EncodeTransfer(1.0 / 12.0, TransferFunction::HLG)
          == Approx(0.5).epsilon(1e-6));

    CHECK(EncodeTransfer(1.0, TransferFunction::HLG)
          == Approx(1.0).epsilon(1e-6));

    for (double x : {0.0, 0.01, 1.0 / 12.0, 0.25, 0.5, 1.0}) {
        CHECK(DecodeTransfer(EncodeTransfer(x, TransferFunction::HLG),
                             TransferFunction::HLG)
              == Approx(x).epsilon(1e-6).margin(1e-9));
    }

    CHECK_THROWS_AS(EncodeTransfer(1.1, TransferFunction::HLG),
                    std::out_of_range);
    CHECK_THROWS_AS(DecodeTransfer(1.1, TransferFunction::HLG),
                    std::out_of_range);
}

TEST_CASE("Transfer functions reject non-finite values",
          "[ColorSpace][Validation]") {
    const double nan = std::numeric_limits<double>::quiet_NaN();
    const double inf = std::numeric_limits<double>::infinity();

    CHECK_THROWS_AS(DecodeTransfer(nan, TransferFunction::BT709),
                    std::out_of_range);
    CHECK_THROWS_AS(DecodeTransfer(inf, TransferFunction::PQ),
                    std::out_of_range);
    CHECK_THROWS_AS(EncodeTransfer(nan, TransferFunction::SRGB),
                    std::out_of_range);
    CHECK_THROWS_AS(EncodeTransfer(inf, TransferFunction::HLG),
                    std::out_of_range);
}
