// SPDX-License-Identifier: LGPL-3.0-or-later
#ifndef OPENSHOT_GPU_METRICS_H
#define OPENSHOT_GPU_METRICS_H
#include <string>
#include <memory>
namespace openshot {
class Frame;
/// Bounded, operation counters. Export and playback have independent generations.
std::string GpuMetricsSnapshot(bool exporting = false);
void ResetGpuMetrics(bool exporting = false);
#ifndef SWIG
namespace gpu_metrics {
enum Event { DecodeGpu, DecodeCpu, ImageSource, RenderGpu, RenderCpu,
             Readback, Upload, EncodeGpu, EncodeCpu, Count };
/// Scope follows synchronous timeline work; unrelated background readers are ignored.
struct Counters;
class Scope {
    Scope* previous;
    std::shared_ptr<Counters> counters;
    bool gpu = false, readback = false;
    int exceptions;
    friend void Record(Event event);
public:
    explicit Scope(bool exporting);
    ~Scope();
    Scope(const Scope&) = delete;
    Scope& operator=(const Scope&) = delete;
};
class Reason {
    const char* previous;
public:
    explicit Reason(const char* name);
    ~Reason();
};
void Record(Event event);
void RecordRender(const Frame& frame);
}
#endif
}
#endif
