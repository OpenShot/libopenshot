// SPDX-License-Identifier: LGPL-3.0-or-later
#include "openshot_catch.h"
#include "GpuMetrics.h"
#include "Json.h"
#include <thread>
using namespace openshot;
using namespace openshot::gpu_metrics;
namespace {
Json::Value snapshot(bool exporting = false) {
    Json::Reader reader;
    Json::Value value;
    REQUIRE(reader.parse(GpuMetricsSnapshot(exporting), value));
    return value;
}
}
TEST_CASE("GPU metrics isolate work scopes and exports", "[GpuMetrics]") {
    ResetGpuMetrics(); ResetGpuMetrics(true);
    Record(DecodeCpu); // Unrelated reader/thumbnail work is not playback.
    { Scope scope(false); Record(DecodeGpu); Record(RenderGpu); }
    { Scope scope(true); Record(EncodeGpu); Record(Upload); }
    REQUIRE(snapshot()["decode_cpu"].asUInt64() == 0);
    REQUIRE(snapshot()["decode_gpu"].asUInt64() == 1);
    REQUIRE(snapshot()["encode_gpu"].asUInt64() == 0);
    REQUIRE(snapshot(true)["encode_gpu"].asUInt64() == 1);
    REQUIRE(snapshot()["copy_free"].asUInt64() == 1);
}
TEST_CASE("GPU metrics count nested work once and attribute readbacks", "[GpuMetrics]") {
    ResetGpuMetrics();
    { Scope outer(false); Record(RenderGpu);
      { Scope nested(false); Reason reason("Effect"); Record(Readback); }
    }
    auto value = snapshot();
    REQUIRE(value["gpu_requests"].asUInt64() == 1);
    REQUIRE(value["copy_free"].asUInt64() == 0);
    REQUIRE(value["readback_reasons"]["Effect"].asUInt64() == 1);
}
TEST_CASE("GPU metric reset excludes old in-flight work", "[GpuMetrics]") {
    ResetGpuMetrics();
    { Scope old(false); Record(RenderGpu); ResetGpuMetrics(); Record(Readback); }
    REQUIRE(snapshot()["gpu_requests"].asUInt64() == 0);
    REQUIRE(snapshot()["readbacks"].asUInt64() == 0);
    { Scope fresh(false); Record(RenderGpu); }
    REQUIRE(snapshot()["copy_free"].asUInt64() == 1);
}
TEST_CASE("GPU counters support parallel requests", "[GpuMetrics]") {
    ResetGpuMetrics();
    auto work = [] { for (int i = 0; i < 1000; ++i) { Scope scope(false); Record(DecodeGpu); } };
    std::thread first(work), second(work); first.join(); second.join();
    REQUIRE(snapshot()["decode_gpu"].asUInt64() == 2000);
    REQUIRE(snapshot()["gpu_requests"].asUInt64() == 2000);
}

TEST_CASE("Failed GPU work is not a copy-free success", "[GpuMetrics]") {
    ResetGpuMetrics();
    try { Scope scope(false); Record(DecodeGpu); throw 1; } catch (int) {}
    REQUIRE(snapshot()["gpu_requests"].asUInt64() == 0);
}
