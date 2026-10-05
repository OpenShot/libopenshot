// SPDX-FileCopyrightText: 2026 OpenShot Studios, LLC
//
// SPDX-License-Identifier: LGPL-3.0-or-later
#include "openshot_catch.h"
#include "Qt/AdaptivePreroll.h"
using openshot::AdaptivePreroll;

TEST_CASE("adaptive startup uses first cheap samples", "[preroll]") {
    AdaptivePreroll policy;
    policy.Reset(1000);
    CHECK(policy.Target(30, 1, 100, 30, 60) == 30);
    CHECK_FALSE(policy.Ready(2000, 2, true, 30));
    for (int i = 0; i < 3; ++i) policy.Observe(1000);
    CHECK(policy.Target(30, 1, 100, 30, 60) == 2);
    CHECK(policy.Ready(4000, 2, true, 2));
    CHECK_FALSE(policy.Ready(5000, 0, false, 2));
    CHECK(policy.Ready(6000, 1, true, 2));
}
TEST_CASE("adaptive variability increases headroom with hysteresis", "[preroll]") {
    AdaptivePreroll policy;
    policy.Reset(0);
    for (int i = 0; i < 3; ++i) policy.Observe(1000);
    CHECK(policy.Target(30, 1, 100, 30, 60) == 2);
    policy.Observe(60000);
    const auto peak = policy.Target(30, 1, 100, 30, 60);
    CHECK(peak > 2);
    policy.Observe(1000);
    const auto next = policy.Target(30, 1, 100, 30, 60);
    CHECK(next <= peak);
    CHECK(next >= 2);
    for (int i = 0; i < 30; ++i) policy.Observe(1000);
    CHECK(policy.Target(30, 1, 100, 30, 60) == 2);
}
TEST_CASE("slow producer cannot wait indefinitely for optional headroom", "[preroll]") {
    AdaptivePreroll policy;
    policy.Reset(100);
    for (int i = 0; i < 3; ++i) policy.Observe(90000);
    const auto required = policy.Target(30, 1, 100, 30, 60);
    CHECK(required > 2);
    CHECK_FALSE(policy.Ready(499999, 1, true, required));
    CHECK(policy.Ready(500100, 1, true, required));
    CHECK(policy.TimedOut());
    CHECK_FALSE(policy.Ready(600100, 0, true, required));
    CHECK_FALSE(policy.Ready(600100, 1, false, required));
}
TEST_CASE("capacity speed boundary and discontinuity clamp adaptive target", "[preroll]") {
    AdaptivePreroll policy;
    policy.Reset(0);
    for (int i = 0; i < 3; ++i) policy.Observe(1000);
    CHECK(policy.Target(30, -3, 100, 30, 60) == 6);
    CHECK(policy.Target(30, 3, 1, 30, 60) == 1);
    CHECK(policy.Target(30, 3, 0, 30, 60) == 0);
    CHECK(policy.Ready(1, 0, true, 0, 0));
    policy.Reset(1000000);
    CHECK_FALSE(policy.Ready(1000001, 2, true, 30));
    CHECK(policy.Target(30, 1, 100, 30, 60) == 30);
    CHECK(policy.Target(30, 1, 10000, 10000, 10000) == AdaptivePreroll::MaxProbeFrames);
    CHECK(policy.Target(30, 1, 100, -1, 60) == 0);
}

TEST_CASE("fake-clock startup comparison separates latency from sustained throughput", "[preroll]") {
    auto startup = [](int64_t decode_us, bool adaptive) {
        AdaptivePreroll policy;
        policy.Reset(0);
        int64_t now = 0;
        for (int64_t frames = 1; frames < 100; ++frames) {
            now += decode_us;
            policy.Observe(decode_us);
            const auto target = policy.Target(30,1,100,30,60);
            if (adaptive ? policy.Ready(now,frames-1,true,target) : frames >= 31) return now;
        }
        return int64_t(-1);
    };
    CHECK(startup(1000,false) == 31000);
    CHECK(startup(1000,true) == 3000);
    CHECK(startup(55000,false) == 1705000);
    CHECK(startup(55000,true) <= AdaptivePreroll::StartupLimitUs + 55000);
    // 55ms production still exceeds 33ms consumption after either start.
}
