// Copyright (c) 2026 OpenShot Studios, LLC
// SPDX-License-Identifier: LGPL-3.0-or-later
#include "GpuMetrics.h"
#include "VulkanFrameImage.h"
#include "GpuDevices.h"
#include "Logger.h"
#include "GpuDebug.h"
#include <map>
#include <atomic>
#include "VulkanCompositor.h"
#ifdef __APPLE__
#include "MacVulkanInterop.h"
#endif
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
// Readback failures are rare and hard to reproduce, so log what FFmpeg was given.
void LogReadbackFailure(const AVFrame* frame, int result) {
    auto fc = reinterpret_cast<const AVHWFramesContext*>(frame->hw_frames_ctx->data);
    auto vfc = static_cast<const AVVulkanFramesContext*>(fc->hwctx);
    auto vk = static_cast<const AVVulkanDeviceContext*>(fc->device_ctx->hwctx);
    // FFmpeg translates failed submissions to AVERROR_EXTERNAL. Confirm the
    // VkResult here so callers stop using the dead device immediately.
    ReportVulkanResult(vkDeviceWaitIdle(vk->act_dev));
    bool hostImport = false;
    for (int i = 0; i < vk->nb_enabled_dev_extensions; ++i)
        hostImport |= std::strcmp(vk->enabled_dev_extensions[i], VK_EXT_EXTERNAL_MEMORY_HOST_EXTENSION_NAME) == 0;
    VkPhysicalDeviceExternalMemoryHostPropertiesEXT host{VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_EXTERNAL_MEMORY_HOST_PROPERTIES_EXT};
    VkPhysicalDeviceProperties2 properties{VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_PROPERTIES_2, &host};
    vkGetPhysicalDeviceProperties2(vk->phys_dev, &properties);
    char error[AV_ERROR_MAX_STRING_SIZE];
    av_strerror(result, error, sizeof(error));
    const char* format = av_get_pix_fmt_name(fc->sw_format);
    Logger::Instance()->Log(std::string("Vulkan readback failed: ") + error + " (" + std::to_string(result) +
        "); frame " + std::to_string(frame->width) + "x" + std::to_string(frame->height) + "; pool " +
        std::to_string(fc->width) + "x" + std::to_string(fc->height) + " " + (format ? format : "unknown") +
        "; Vulkan format " + std::to_string(vfc->format[0]) + "; tiling " + std::to_string(vfc->tiling) +
        "; usage 0x" + QString::number(vfc->usage, 16).toStdString() + "; host memory import " +
        (hostImport ? "on, alignment " + std::to_string(host.minImportedHostPointerAlignment) : std::string("off")) +
        "; row pitch alignment " + std::to_string(properties.properties.limits.optimalBufferCopyRowPitchAlignment) +
        "; device " + properties.properties.deviceName, Logger::LevelWarning);
}
void Validate(const AVFrame* frame) {
    if (!frame || frame->format != AV_PIX_FMT_VULKAN || !frame->hw_frames_ctx ||
        !frame->data[0] || frame->width <= 0 || frame->height <= 0)
        throw std::invalid_argument("Expected an FFmpeg Vulkan hardware frame");
}
}
namespace { std::atomic<uint64_t> readback_count{0}, upload_failures{0}; }
uint64_t VulkanFrameReadbackCount() { return readback_count.load(std::memory_order_relaxed); }
uint64_t VulkanUploadFailureCount() { return upload_failures.load(std::memory_order_relaxed); }
void RecordVulkanUploadFailure() {
    upload_failures.fetch_add(1, std::memory_order_relaxed);
    gpu_metrics::Record(gpu_metrics::UploadFailure);
}
bool VulkanRenderingEnabled() {
    const char* backend = std::getenv("OPENSHOT_RENDER_BACKEND");
    return !(backend && std::strcmp(backend, "cpu") == 0) && !VulkanDeviceLost();
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
#ifdef __APPLE__
    if (IsMacVulkanFrame(impl->frame.get())) return MacVulkanFrameBytes(impl->frame.get());
#endif
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
#ifdef __APPLE__
    if (IsMacVulkanFrame(impl->frame.get()))
        Check(ReadMacVulkanFrame(cpu.get(), impl->frame.get()), "Read back Mac Vulkan frame");
    else
#endif
    {
        const int result = av_hwframe_transfer_data(cpu.get(), impl->frame.get(), 0);
        if (result < 0) LogReadbackFailure(impl->frame.get(), result);
        Check(result, "Read back Vulkan frame");
    }
    const auto readbacks = readback_count.fetch_add(1,std::memory_order_relaxed) + 1;
    gpu_metrics::Record(gpu_metrics::Readback);
    // GpuDebug: say why frames are copied back ("Other" is work outside the
    // timeline, such as thumbnails), for the first 20 and every 100th.
    if (GpuDebug() && (readbacks <= 20 || readbacks % 100 == 0))
        Logger::Instance()->Log("GPU debug readback " + std::to_string(readbacks) + ": reason " +
            gpu_metrics::CurrentReason() + "; " + std::to_string(GetWidth()) + "x" + std::to_string(GetHeight()),
            Logger::LevelInfo);
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
#ifdef __APPLE__
    OwnedFrame imported;
    if (frame && frame->format == AV_PIX_FMT_VIDEOTOOLBOX) {
        imported.reset(ImportVideoToolboxFrame(frame));
        frame = imported.get();
    }
#endif
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
namespace {
struct BufferDeleter { void operator()(AVBufferRef* p) const { av_buffer_unref(&p); } };
using OwnedBuffer = std::unique_ptr<AVBufferRef, BufferDeleter>;
struct UploadCache {
    struct Entry { std::shared_ptr<const FrameImage> surface; int64_t bytes; uint64_t age; };
    struct Pool { OwnedBuffer frames; uint64_t age; };
    std::mutex mutex;
    std::map<qint64, Entry> entries;
    // One frame pool per image size. A pool per upload leaked its device
    // memory on NVIDIA (about 1.5 MiB per 600x600 upload) and was slow.
    std::map<std::pair<int, int>, Pool> pools;
    int64_t bytes = 0;
    uint64_t clock = 0;
};
// Never destroyed: freeing cached Vulkan frames from a static destructor at
// process exit crashes the Windows NVIDIA driver, which may already be unloading.
UploadCache& Uploads() { static auto* cache = new UploadCache; return *cache; }
AVBufferRef* UploadPool(UploadCache& cache, int width, int height) {
    auto found = cache.pools.find({width, height});
    if (found != cache.pools.end()) {
        found->second.age = ++cache.clock;
        return found->second.frames.get();
    }
    AVBufferRef* device_ptr = nullptr;
    Check(CreateSharedVulkanDevice(&device_ptr), "Create image upload device");
    OwnedBuffer device(device_ptr);
    OwnedBuffer pool(av_hwframe_ctx_alloc(device.get()));
    if (!pool) throw std::bad_alloc();
    auto* context = reinterpret_cast<AVHWFramesContext*>(pool->data);
    context->format = AV_PIX_FMT_VULKAN;
    context->sw_format = AV_PIX_FMT_RGBA;
    context->width = width; context->height = height;
    auto* vk = reinterpret_cast<AVVulkanFramesContext*>(context->hwctx);
    vk->usage = static_cast<VkImageUsageFlagBits>(VK_IMAGE_USAGE_SAMPLED_BIT |
        VK_IMAGE_USAGE_STORAGE_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT);
    Check(av_hwframe_ctx_init(pool.get()), "Initialize image upload pool");
    // Images still in use keep their own pool reference after it is dropped here.
    if (cache.pools.size() >= 8)
        cache.pools.erase(std::min_element(cache.pools.begin(), cache.pools.end(),
            [](const auto& a, const auto& b) { return a.second.age < b.second.age; }));
    auto* frames = pool.get();
    cache.pools.emplace(std::make_pair(width, height), UploadCache::Pool{std::move(pool), ++cache.clock});
    return frames;
}
}
std::shared_ptr<const FrameImage> UploadVulkanImage(const QImage& image) {
    if (image.isNull()) throw std::invalid_argument("Cannot upload an empty image");
    auto& cache = Uploads();
    const std::lock_guard<std::mutex> lock(cache.mutex);
    const auto key = image.cacheKey();
    auto found = cache.entries.find(key);
    if (found != cache.entries.end()) {
        found->second.age = ++cache.clock;
        return found->second.surface;
    }
    auto* pool = UploadPool(cache, image.width(), image.height());
    QImage rgba = image.convertToFormat(QImage::Format_RGBA8888_Premultiplied);
    auto cpu = AllocateFrame();
    auto gpu = AllocateFrame();
    cpu->format = AV_PIX_FMT_RGBA; cpu->width = rgba.width(); cpu->height = rgba.height();
    cpu->data[0] = const_cast<uint8_t*>(rgba.constBits());
    cpu->linesize[0] = rgba.bytesPerLine();
    Check(av_hwframe_get_buffer(pool, gpu.get(), 0), "Allocate uploaded image");
    Check(av_hwframe_transfer_data(gpu.get(), cpu.get(), 0), "Upload CPU image");
    // FFmpeg may copy straight from the CPU pixels (host-mapped memory) and
    // returns before the copy runs; wait so rgba is not freed or reused first.
    auto* uploaded = reinterpret_cast<AVVkFrame*>(gpu->data[0]);
    auto* frames = reinterpret_cast<AVHWFramesContext*>(pool->data);
    auto* device = reinterpret_cast<AVVulkanDeviceContext*>(frames->device_ctx->hwctx);
    VkSemaphoreWaitInfo copied{VK_STRUCTURE_TYPE_SEMAPHORE_WAIT_INFO};
    copied.semaphoreCount = 1;
    copied.pSemaphores = &uploaded->sem[0];
    copied.pValues = &uploaded->sem_value[0];
    const VkResult waited = vkWaitSemaphores(device->act_dev, &copied, UINT64_MAX);
    ReportVulkanResult(waited);
    if (waited != VK_SUCCESS)
        throw std::runtime_error("Wait for CPU image upload: " + VulkanResultName(waited));
    gpu_metrics::Record(gpu_metrics::Upload);
    std::shared_ptr<const FrameImage> surface = std::make_shared<VulkanFrameImage>(gpu.get(), true);
#ifdef __APPLE__
    // Cache an IOSurface-backed image, so CPU fallback uses the synchronized
    // Metal bridge instead of FFmpeg's generic MoltenVK staging-buffer readback.
    surface = RenderVulkanImage(surface, nullptr, image.width(), image.height(),
                               QTransform(), 1.0, Qt::transparent);
#endif
    const int64_t size = surface->GetBytes();
    constexpr int64_t budget = 64 * 1024 * 1024;
    if (size <= budget) {
        while (!cache.entries.empty() && (cache.bytes + size > budget || cache.entries.size() >= 16)) {
            auto oldest = std::min_element(cache.entries.begin(), cache.entries.end(),
                [](const auto& a, const auto& b) { return a.second.age < b.second.age; });
            cache.bytes -= oldest->second.bytes;
            cache.entries.erase(oldest);
        }
        cache.entries.emplace(key, UploadCache::Entry{surface, size, ++cache.clock});
        cache.bytes += size;
    }
    return surface;
}
void ReleaseVulkanUploads() {
    auto& cache = Uploads();
    const std::lock_guard<std::mutex> lock(cache.mutex);
    cache.entries.clear(); cache.bytes = 0;
    cache.pools.clear();
}

}
