
// Copyright (c) 2008-2026 OpenShot Studios, LLC
// SPDX-License-Identifier: LGPL-3.0-or-later

#include "openshot_catch.h"
#include "AudioReactiveKeyframes.h"

#include <cmath>
#include <limits>
#include <stdexcept>
#include <vector>

using namespace openshot;

TEST_CASE("Audio reactive silence", "[libopenshot][audioreactive]")
{
    Keyframe base(1.0);
    const std::vector<float> energy{0.0f, 0.0f, 0.0f};

    const auto result = AudioReactiveKeyframes::Generate(
        energy, base, AudioReactiveOptions{});

    CHECK(result.GetCount() == 3);
    CHECK(result.GetValue(1) == Approx(1.0));
    CHECK(result.GetValue(3) == Approx(1.0));
}

TEST_CASE("Audio reactive scale peak", "[libopenshot][audioreactive]")
{
    Keyframe base(1.0);
    const std::vector<float> energy{0.0f, 0.5f, 1.0f};

    const auto result = AudioReactiveKeyframes::Generate(
        energy, base, AudioReactiveOptions{});

    CHECK(result.GetValue(1) == Approx(1.0));
    CHECK(result.GetValue(2) == Approx(1.125));
    CHECK(result.GetValue(3) == Approx(1.25));
}

TEST_CASE("Audio reactive threshold", "[libopenshot][audioreactive]")
{
    Keyframe base(1.0);

    AudioReactiveOptions options;
    options.threshold = 0.5;

    const auto result = AudioReactiveKeyframes::Generate(
        {0.2f, 0.5f, 1.0f}, base, options);

    CHECK(result.GetValue(1) == Approx(1.0));
    CHECK(result.GetValue(2) == Approx(1.0));
    CHECK(result.GetValue(3) == Approx(1.25));
}

TEST_CASE("Audio reactive smoothing", "[libopenshot][audioreactive]")
{
    Keyframe base(1.0);

    AudioReactiveOptions options;
    options.smoothing_frames = 2;

    const auto result = AudioReactiveKeyframes::Generate(
        {0.0f, 1.0f, 0.0f}, base, options);

    CHECK(result.GetValue(1) == Approx(1.0));
    CHECK(result.GetValue(2) == Approx(1.125));
    CHECK(result.GetValue(3) == Approx(1.125));
}

TEST_CASE("Audio reactive location", "[libopenshot][audioreactive]")
{
    Keyframe base(0.2);

    AudioReactiveOptions options;
    options.mode = AudioReactiveMode::LOCATION;
    options.amount = 0.4;

    const auto result = AudioReactiveKeyframes::Generate(
        {0.0f, 1.0f}, base, options);

    CHECK(result.GetValue(1) == Approx(0.2));
    CHECK(result.GetValue(2) == Approx(0.6));
}

TEST_CASE("Audio reactive preserves original curve",
          "[libopenshot][audioreactive]")
{
    Keyframe base;
    base.AddPoint(1, 1.0, LINEAR);
    base.AddPoint(3, 2.0, LINEAR);

    const auto result = AudioReactiveKeyframes::Generate(
        {0.0f, 1.0f, 0.0f},
        base,
        AudioReactiveOptions{});

    CHECK(base.GetCount() == 2);
    CHECK(base.GetValue(2) == Approx(1.5));
    CHECK(result.GetValue(2) == Approx(1.875));
    CHECK(result.GetValue(3) == Approx(2.0));
}

TEST_CASE("Audio reactive empty input", "[libopenshot][audioreactive]")
{
    Keyframe base;
    base.AddPoint(1, 1.0, LINEAR);
    base.AddPoint(3, 2.0, LINEAR);

    const auto result = AudioReactiveKeyframes::Generate(
        {}, base, AudioReactiveOptions{});

    CHECK(result.GetCount() == base.GetCount());
    CHECK(result.GetValue(2) == Approx(1.5));
}

TEST_CASE("Audio reactive rejects invalid options",
          "[libopenshot][audioreactive]")
{
    Keyframe base(1.0);
    AudioReactiveOptions options;

    options.threshold = 1.0;

    CHECK_THROWS_AS(
        AudioReactiveKeyframes::Generate({1.0f}, base, options),
        std::invalid_argument);

    options.threshold = 0.0;
    options.smoothing_frames = 0;

    CHECK_THROWS_AS(
        AudioReactiveKeyframes::Generate({1.0f}, base, options),
        std::invalid_argument);
}



TEST_CASE("Audio reactive non-finite energy",
          "[libopenshot][audioreactive]")
{
    Keyframe base(1.0);

    const float nan =
        std::numeric_limits<float>::quiet_NaN();
    const float inf =
        std::numeric_limits<float>::infinity();

    const auto result = AudioReactiveKeyframes::Generate(
        {nan, inf, -inf, 1.0f},
        base,
        AudioReactiveOptions{});

    CHECK(result.GetValue(1) == Approx(1.0));
    CHECK(result.GetValue(2) == Approx(1.0));
    CHECK(result.GetValue(3) == Approx(1.0));
    CHECK(result.GetValue(4) == Approx(1.25));
}

TEST_CASE("Audio reactive long smoothing window",
          "[libopenshot][audioreactive]")
{
    Keyframe base(1.0);

    AudioReactiveOptions options;
    options.smoothing_frames = 100;

    const auto result = AudioReactiveKeyframes::Generate(
        {0.0f, 1.0f, 0.0f},
        base,
        options);

    CHECK(result.GetValue(1) == Approx(1.0));
    CHECK(result.GetValue(2) == Approx(1.125));
    CHECK(result.GetValue(3) == Approx(1.0 + 0.25 / 3.0));
}

TEST_CASE("Audio reactive rejects invalid mode",
          "[libopenshot][audioreactive]")
{
    Keyframe base(1.0);

    AudioReactiveOptions options;
    options.mode = static_cast<AudioReactiveMode>(999);

    CHECK_THROWS_AS(
        AudioReactiveKeyframes::Generate({1.0f}, base, options),
        std::invalid_argument);
}

TEST_CASE("Audio reactive large gain remains finite",
          "[libopenshot][audioreactive]")
{
    Keyframe base(1.0);

    AudioReactiveOptions options;
    options.gain = std::numeric_limits<double>::max();

    const auto result = AudioReactiveKeyframes::Generate(
        {1.0f}, base, options);

    CHECK(std::isfinite(result.GetValue(1)));
    CHECK(result.GetValue(1) == Approx(1.25));
}

TEST_CASE("Audio reactive rejects overflowing output",
          "[libopenshot][audioreactive]")
{
    const double large =
        std::numeric_limits<double>::max();

    Keyframe base(large);
    AudioReactiveOptions options;

    options.amount = large;
    options.mode = AudioReactiveMode::SCALE;

    CHECK_THROWS_AS(
        AudioReactiveKeyframes::Generate(
            {1.0f}, base, options),
        std::overflow_error);
}
