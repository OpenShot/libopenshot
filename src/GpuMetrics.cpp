// SPDX-License-Identifier: LGPL-3.0-or-later
#include "GpuMetrics.h"
#include "Frame.h"
#include "Json.h"
#include <array>
#include <atomic>
#include <memory>
#include <cstring>
#include <exception>
namespace openshot { namespace gpu_metrics {
struct Counters {
    std::array<std::atomic<uint64_t>, Count> values{};
    std::atomic<uint64_t> resident{0}, requests{0};
    std::array<std::atomic<uint64_t>, 5> reasons{};
};
namespace {
std::shared_ptr<Counters> sessions[2] = {std::make_shared<Counters>(), std::make_shared<Counters>()};
thread_local Scope* current = nullptr;
thread_local const char* reason = "Other";
const char* names[] = {"decode_gpu", "decode_cpu", "image_sources", "render_gpu", "render_cpu",
                       "readbacks", "uploads", "encode_gpu", "encode_cpu"};
const char* reasons[] = {"Effect", "Transform", "Composite", "Encoder", "Other"};
}
Scope::Scope(bool exporting) : previous(current), exceptions(std::uncaught_exceptions()) {
    if (!current) {
        counters = std::atomic_load(&sessions[exporting ? 1 : 0]);
        current = this;
    }
}
Scope::~Scope() {
    if (counters) {
        if (gpu && std::uncaught_exceptions() == exceptions) {
            counters->requests.fetch_add(1, std::memory_order_relaxed);
            if (!readback) counters->resident.fetch_add(1, std::memory_order_relaxed);
        }
        current = previous;
    }
}
Reason::Reason(const char* name) : previous(reason) { reason = name; }
Reason::~Reason() { reason = previous; }
void Record(Event event) {
    if (!current) return;
    current->counters->values[event].fetch_add(1, std::memory_order_relaxed);
    if (event == DecodeGpu || event == RenderGpu || event == EncodeGpu) current->gpu = true;
    if (event == Readback) {
        current->readback = true;
        size_t index = 4;
        for (size_t i = 0; i < 4; ++i) if (std::strcmp(reason, reasons[i]) == 0) { index = i; break; }
        current->counters->reasons[index].fetch_add(1, std::memory_order_relaxed);
    }
}
void RecordRender(const Frame& frame) {
#ifdef OPENSHOT_ENABLE_GPU_FRAME
    Record(frame.GetDeferredImage() ? RenderGpu : RenderCpu);
#else
    Record(RenderCpu);
#endif
}
} // namespace gpu_metrics
std::string GpuMetricsSnapshot(bool exporting) {
    using namespace gpu_metrics;
    auto counters = std::atomic_load(&sessions[exporting ? 1 : 0]);
    Json::Value root(Json::objectValue);
    for (int i = 0; i < Count; ++i) root[names[i]] = Json::UInt64(counters->values[i].load(std::memory_order_relaxed));
    root["copy_free"] = Json::UInt64(counters->resident.load(std::memory_order_relaxed));
    root["gpu_requests"] = Json::UInt64(counters->requests.load(std::memory_order_relaxed));
    for (int i = 0; i < 5; ++i) root["readback_reasons"][reasons[i]] = Json::UInt64(counters->reasons[i].load(std::memory_order_relaxed));
    root["definition"] = "Operation counts exclude cache hits; copy-free counts GPU work requests without GPU-to-CPU readbacks, not end-to-end host-free frames";
    return root.toStyledString();
}
void ResetGpuMetrics(bool exporting) {
    std::atomic_store(&gpu_metrics::sessions[exporting ? 1 : 0], std::make_shared<gpu_metrics::Counters>());
}
}
