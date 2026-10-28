// Copyright (c) 2026 OpenShot Studios, LLC
// SPDX-License-Identifier: LGPL-3.0-or-later
#include "VulkanFrameImage.h"
#include "VulkanCompositor.h"
#include <mutex>
#include <algorithm>
#include <cstdlib>
#include <cstring>
#include <limits>
#include <stdexcept>
#include <string>
extern "C" {
#include <libavfilter/avfilter.h>
#include <libavfilter/buffersrc.h>
#include <libavfilter/buffersink.h>
#include <libavutil/hwcontext.h>
#include <libavutil/hwcontext_vulkan.h>
#include <libavutil/error.h>
#include <libavutil/pixdesc.h>
#include <libswscale/swscale.h>
}
namespace openshot {
namespace {
void Check(int result, const char* operation) {
    if (result >= 0) return;
    char error[AV_ERROR_MAX_STRING_SIZE];
    av_strerror(result, error, sizeof(error));
    throw std::runtime_error(std::string(operation) + ": " + error);
}
struct FrameDeleter { void operator()(AVFrame* f) const { av_frame_free(&f); } };
using OwnedFrame = std::unique_ptr<AVFrame, FrameDeleter>;
OwnedFrame AllocateFrame() {
    OwnedFrame f(av_frame_alloc());
    if (!f) throw std::bad_alloc();
    return f;
}
void Validate(const AVFrame* frame) {
    if (!frame || frame->format != AV_PIX_FMT_VULKAN || !frame->hw_frames_ctx ||
        !frame->data[0] || frame->width <= 0 || frame->height <= 0)
        throw std::invalid_argument("Expected an FFmpeg Vulkan hardware frame");
}
}
bool VulkanRenderingEnabled() {
    const char* backend = std::getenv("OPENSHOT_RENDER_BACKEND");
    return !(backend && std::strcmp(backend, "cpu") == 0);
}
struct VulkanFrameImage::Impl {
    OwnedFrame frame;
    mutable std::mutex mutex;
    bool premultiplied = false;
    explicit Impl(const AVFrame* source) : frame(AllocateFrame()) {
        Validate(source);
        Check(av_frame_ref(frame.get(), source), "Retain Vulkan frame");
    }
};
VulkanFrameImage::VulkanFrameImage(const AVFrame* frame, bool premultiplied) : impl(new Impl(frame)) { impl->premultiplied = premultiplied; }
bool VulkanFrameImage::IsPremultiplied() const { return impl->premultiplied; }
VulkanFrameImage::~VulkanFrameImage() = default;
int VulkanFrameImage::GetWidth() const { return impl->frame->width; }
int VulkanFrameImage::GetHeight() const { return impl->frame->height; }
const AVFrame* VulkanFrameImage::GetFrame() const { return impl->frame.get(); }
int64_t VulkanFrameImage::GetBytes() const {
    const auto* frame = reinterpret_cast<const AVVkFrame*>(impl->frame->data[0]);
    int64_t bytes = 0;
    for (int i = 0; i < AV_NUM_DATA_POINTERS; ++i) {
        if (frame->mem[i] == VK_NULL_HANDLE) continue;
        bool counted = false;
        for (int j = 0; j < i; ++j) counted |= frame->mem[j] == frame->mem[i];
        if (counted) continue;
        // Planes may share one allocation. Count its full padded allocation once.
        size_t allocation = frame->size[i];
        for (int j = i + 1; j < AV_NUM_DATA_POINTERS; ++j)
            if (frame->mem[j] == frame->mem[i]) allocation = std::max(allocation, frame->size[j]);
        if (allocation > static_cast<uint64_t>(std::numeric_limits<int64_t>::max() - bytes))
            return std::numeric_limits<int64_t>::max();
        bytes += static_cast<int64_t>(allocation);
    }
    return bytes;
}
QImage VulkanFrameImage::Materialize() const {
    std::lock_guard<std::mutex> lock(impl->mutex);
    auto cpu = AllocateFrame();
    Check(av_hwframe_transfer_data(cpu.get(), impl->frame.get(), 0), "Read back Vulkan frame");
    Check(av_frame_copy_props(cpu.get(), impl->frame.get()), "Copy Vulkan frame metadata");
    QImage result(GetWidth(), GetHeight(), QImage::Format_RGBA8888);
    if (result.isNull()) throw std::bad_alloc();
    auto* scaler = sws_getContext(cpu->width, cpu->height, AVPixelFormat(cpu->format),
        result.width(), result.height(), AV_PIX_FMT_RGBA, SWS_BILINEAR, nullptr, nullptr, nullptr);
    if (!scaler) throw std::runtime_error("Unable to create Vulkan readback color converter");
    std::unique_ptr<SwsContext, decltype(&sws_freeContext)> owned_scaler(scaler, sws_freeContext);
    int space = SWS_CS_DEFAULT;
    switch (cpu->colorspace) {
        case AVCOL_SPC_BT709: space = SWS_CS_ITU709; break;
        case AVCOL_SPC_BT2020_NCL: case AVCOL_SPC_BT2020_CL: space = SWS_CS_BT2020; break;
        case AVCOL_SPC_SMPTE240M: space = SWS_CS_SMPTE240M; break;
        default: break;
    }
    const int* coefficients = sws_getCoefficients(space);
    Check(sws_setColorspaceDetails(scaler, coefficients, cpu->color_range == AVCOL_RANGE_JPEG,
        coefficients, 1, 0, 1 << 16, 1 << 16), "Set readback color conversion");
    uint8_t* output[] = {result.bits(), nullptr, nullptr, nullptr};
    int strides[] = {result.bytesPerLine(), 0, 0, 0};
    Check(sws_scale(scaler, cpu->data, cpu->linesize, 0, cpu->height, output, strides), "Convert readback pixels");
    if (impl->premultiplied) return QImage(result.constBits(), result.width(), result.height(), result.bytesPerLine(), QImage::Format_RGBA8888_Premultiplied).copy();
    return result.convertToFormat(QImage::Format_RGBA8888_Premultiplied);
}
struct VulkanFrameProcessor::Impl {};
VulkanFrameProcessor::VulkanFrameProcessor() : impl(new Impl) {}
VulkanFrameProcessor::~VulkanFrameProcessor() = default;
void VulkanFrameProcessor::Reset() {}
std::shared_ptr<const FrameImage> VulkanFrameProcessor::Process(const AVFrame* frame, int width, int height, bool high_quality) {
    Validate(frame);
    if (width <= 0 || height <= 0) throw std::invalid_argument("Invalid Vulkan output dimensions");
    // Preserve the existing FFmpegReader conversion matrix (SWS_CS_DEFAULT / BT.601).
    // The generic compositor still honors metadata for callers outside that reader.
    auto compatible = AllocateFrame();
    Check(av_frame_ref(compatible.get(), frame), "Reference reader Vulkan frame");
    compatible->colorspace = AVCOL_SPC_SMPTE170M;
    auto source = std::make_shared<VulkanFrameImage>(compatible.get());
    QTransform matrix;
    matrix.scale(double(width) / frame->width, double(height) / frame->height);
    return RenderVulkanImage(source, nullptr, width, height, matrix, 1.0, Qt::transparent, high_quality);
}
}
