// Copyright (c) 2026 OpenShot Studios, LLC
// SPDX-License-Identifier: LGPL-3.0-or-later
#define VK_USE_PLATFORM_METAL_EXT
#import <Foundation/Foundation.h>
#import <Metal/Metal.h>
#import <CoreVideo/CoreVideo.h>
#include "MacVulkanInterop.h"
#include "GpuDevices.h"
#include <algorithm>
#include <array>
#include <map>
#include <limits>
#include <memory>
#include <mutex>
#include <stdexcept>
#include <tuple>
extern "C" {
#include <libavutil/hwcontext.h>
#include <libavutil/hwcontext_vulkan.h>
#include <libavutil/imgutils.h>
#include <libavutil/error.h>
}
namespace openshot {
namespace {
const char native_pool_tag = 0;
void AvCheck(int r, const char* what) { if (r < 0) throw std::runtime_error(what); }
void VkCheck(VkResult r, const char* what) { if (r != VK_SUCCESS) throw std::runtime_error(what); }
struct Pools {
    std::mutex mutex;
    std::map<std::tuple<AVHWDeviceContext*, int, int, bool>, AVBufferRef*> frames;
    ~Pools() { for (auto& pair : frames) av_buffer_unref(&pair.second); }
};
Pools& NativePools() { static Pools pools; return pools; }
AVBufferRef* Pool(AVBufferRef* device, int width, int height, bool nv12) {
    auto& pools = NativePools();
    std::lock_guard<std::mutex> lock(pools.mutex);
    auto key = std::make_tuple(reinterpret_cast<AVHWDeviceContext*>(device->data), width, height, nv12);
    auto& ref = pools.frames[key];
    if (!ref) {
        ref = av_hwframe_ctx_alloc(device);
        if (!ref) throw std::bad_alloc();
        auto* fc = reinterpret_cast<AVHWFramesContext*>(ref->data);
        fc->format = AV_PIX_FMT_VULKAN;
        fc->sw_format = nv12 ? AV_PIX_FMT_NV12 : AV_PIX_FMT_RGBA;
        fc->width = width; fc->height = height;
        fc->user_opaque = const_cast<char*>(&native_pool_tag);
        auto* vc = static_cast<AVVulkanFramesContext*>(fc->hwctx);
        vc->flags = AV_VK_FRAME_FLAG_DISABLE_MULTIPLANE;
        vc->usage = static_cast<VkImageUsageFlagBits>(VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_STORAGE_BIT |
            VK_IMAGE_USAGE_TRANSFER_SRC_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT);
        const int result = av_hwframe_ctx_init(ref);
        if (result < 0) { av_buffer_unref(&ref); pools.frames.erase(key); AvCheck(result, "Initialize Mac Vulkan frame pool"); }
    }
    AVBufferRef* result = av_buffer_ref(ref);
    if (!result) throw std::bad_alloc();
    return result;
}
struct NativeFrame {
    AVFrame* allocation = nullptr;
    AVVkFrame original{};
    AVVkFrame* image = nullptr;
    AVHWFramesContext* fc = nullptr;
    AVVulkanFramesContext* vc = nullptr;
    AVVulkanDeviceContext* vk = nullptr;
    CVPixelBufferRef pixel = nullptr;
    CVMetalTextureCacheRef cache = nullptr;
    std::array<CVMetalTextureRef, 2> textures{};
    std::array<id<MTLTexture>, 2> direct_textures{};
    id<MTLTexture> Texture(int plane) const {
        return direct_textures[plane] ?: CVMetalTextureGetTexture(textures[plane]);
    }
    std::array<VkImage, 2> imported{};
    uint64_t registry_id = 0;
    int count = 0;
    bool substituted = false;
    void Wait() const {
        vc->lock_frame(fc, image);
        VkSemaphoreWaitInfo wait{VK_STRUCTURE_TYPE_SEMAPHORE_WAIT_INFO};
        wait.semaphoreCount = count;
        wait.pSemaphores = image->sem;
        wait.pValues = image->sem_value;
        const auto result = vkWaitSemaphores(vk->act_dev, &wait, UINT64_MAX);
        vc->unlock_frame(fc, image);
        VkCheck(result, "Wait for Mac surface GPU access");
    }
    ~NativeFrame() {
        if (substituted) {
            // Keep the normal FFmpeg allocation and its private synchronization
            // internals intact. Its pool callback is the only supported way to
            // release an AVVkFrame; no reliance on FFmpeg's private struct ABI.
            try { Wait(); } catch (...) { /* Device loss: retire owned objects. */ }
        }
        if (vk) for (auto img : imported) if (img) vkDestroyImage(vk->act_dev, img, nullptr);
        if (substituted) {
            for (int i = 0; i < count; ++i) original.sem_value[i] = image->sem_value[i];
            *image = original;
        }
        av_frame_free(&allocation);
        for (auto texture : textures) if (texture) CFRelease(texture);
        for (auto texture : direct_textures) [texture release];
        if (cache) CFRelease(cache);
        if (pixel) CVPixelBufferRelease(pixel);
    }
};
NativeFrame* State(const AVFrame* frame) {
    if (!frame || frame->format != AV_PIX_FMT_VULKAN || !frame->hw_frames_ctx || !frame->buf[0]) return nullptr;
    auto* fc = reinterpret_cast<AVHWFramesContext*>(frame->hw_frames_ctx->data);
    if (fc->user_opaque != &native_pool_tag) return nullptr;
    return static_cast<NativeFrame*>(av_buffer_get_opaque(frame->buf[0]));
}
AVFrame* Wrap(AVBufferRef* device, CVPixelBufferRef pixel, bool nv12, bool writable) {
    @autoreleasepool {
        const int width = static_cast<int>(CVPixelBufferGetWidth(pixel));
        const int height = static_cast<int>(CVPixelBufferGetHeight(pixel));
        if (width <= 0 || height <= 0 || !CVPixelBufferGetIOSurface(pixel))
            throw std::invalid_argument("Mac GPU frames require IOSurface-backed pixels");
        auto state = std::make_unique<NativeFrame>();
        state->pixel = CVPixelBufferRetain(pixel);
        state->count = nv12 ? 2 : 1;
        state->allocation = av_frame_alloc();
        if (!state->allocation) throw std::bad_alloc();
        AVBufferRef* pool = Pool(device, width, height, nv12);
        const int allocated = av_hwframe_get_buffer(pool, state->allocation, 0);
        av_buffer_unref(&pool);
        AvCheck(allocated, "Allocate Mac Vulkan frame synchronization");
        state->fc = reinterpret_cast<AVHWFramesContext*>(state->allocation->hw_frames_ctx->data);
        state->vc = static_cast<AVVulkanFramesContext*>(state->fc->hwctx);
        state->vk = static_cast<AVVulkanDeviceContext*>(state->fc->device_ctx->hwctx);
        state->image = reinterpret_cast<AVVkFrame*>(state->allocation->data[0]);
        state->original = *state->image;
        auto exportObjects = reinterpret_cast<PFN_vkExportMetalObjectsEXT>(vkGetDeviceProcAddr(state->vk->act_dev, "vkExportMetalObjectsEXT"));
        if (!exportObjects) throw std::runtime_error("Vulkan device lacks VK_EXT_metal_objects");
        VkExportMetalDeviceInfoEXT metal{VK_STRUCTURE_TYPE_EXPORT_METAL_DEVICE_INFO_EXT};
        VkExportMetalObjectsInfoEXT objects{VK_STRUCTURE_TYPE_EXPORT_METAL_OBJECTS_INFO_EXT};
        objects.pNext = &metal;
        exportObjects(state->vk->act_dev, &objects);
        state->registry_id = metal.mtlDevice.registryID;
        if (!metal.mtlDevice) throw std::runtime_error("No Metal device for Vulkan");
        if (CVMetalTextureCacheCreate(kCFAllocatorDefault, nullptr, metal.mtlDevice, nullptr, &state->cache) != kCVReturnSuccess)
            throw std::runtime_error("Create Core Video Metal texture cache");
        NSDictionary* attributes = @{(id)kCVMetalTextureUsage: @(MTLTextureUsageShaderRead |
            (writable ? MTLTextureUsageShaderWrite : 0))};
        for (int i = 0; i < state->count; ++i) {
            const auto w = nv12 ? CVPixelBufferGetWidthOfPlane(pixel, i) : size_t(width);
            const auto h = nv12 ? CVPixelBufferGetHeightOfPlane(pixel, i) : size_t(height);
            const auto format = nv12 ? (i ? MTLPixelFormatRG8Unorm : MTLPixelFormatR8Unorm) : MTLPixelFormatRGBA8Unorm;
            if (nv12) {
                if (CVMetalTextureCacheCreateTextureFromImage(kCFAllocatorDefault, state->cache, pixel,
                    (CFDictionaryRef)attributes, format, w, h, i, &state->textures[i]) != kCVReturnSuccess)
                    throw std::runtime_error("Map Core Video pixel planes to Metal");
            } else {
                // Core Video cannot allocate Metal-compatible 32RGBA on older
                // Macs. Allocate a 32BGRA IOSurface and give Metal an RGBA view;
                // the compositor and CPU fallback both retain RGBA byte order.
                auto* descriptor = [MTLTextureDescriptor texture2DDescriptorWithPixelFormat:format
                    width:w height:h mipmapped:NO];
                descriptor.usage = MTLTextureUsageShaderRead | MTLTextureUsageShaderWrite;
                state->direct_textures[i] = [metal.mtlDevice newTextureWithDescriptor:descriptor
                    iosurface:CVPixelBufferGetIOSurface(pixel) plane:i];
                if (!state->direct_textures[i]) throw std::runtime_error("Create RGBA IOSurface texture view");
            }
            VkImportMetalTextureInfoEXT texture{VK_STRUCTURE_TYPE_IMPORT_METAL_TEXTURE_INFO_EXT};
            texture.plane = VK_IMAGE_ASPECT_COLOR_BIT;
            texture.mtlTexture = state->Texture(i);
            VkImageCreateInfo create{VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO};
            create.pNext = &texture; create.imageType = VK_IMAGE_TYPE_2D;
            create.format = nv12 ? (i ? VK_FORMAT_R8G8_UNORM : VK_FORMAT_R8_UNORM) : VK_FORMAT_R8G8B8A8_UNORM;
            create.extent = {uint32_t(w), uint32_t(h), 1}; create.mipLevels = create.arrayLayers = 1;
            create.samples = VK_SAMPLE_COUNT_1_BIT; create.tiling = VK_IMAGE_TILING_OPTIMAL;
            create.usage = VK_IMAGE_USAGE_SAMPLED_BIT | (writable ? VK_IMAGE_USAGE_STORAGE_BIT : 0);
            create.initialLayout = writable ? VK_IMAGE_LAYOUT_UNDEFINED : VK_IMAGE_LAYOUT_PREINITIALIZED;
            VkCheck(vkCreateImage(state->vk->act_dev, &create, nullptr, &state->imported[i]), "Import shared Mac texture into Vulkan");
        }
        for (int i = 0; i < state->count; ++i) {
            state->image->img[i] = state->imported[i];
            state->image->mem[i] = VK_NULL_HANDLE;
            state->image->size[i] = nv12 ? CVPixelBufferGetBytesPerRowOfPlane(pixel, i) * CVPixelBufferGetHeightOfPlane(pixel, i)
                : CVPixelBufferGetBytesPerRow(pixel) * height;
            state->image->offset[i] = 0;
            state->image->layout[i] = writable ? VK_IMAGE_LAYOUT_UNDEFINED : VK_IMAGE_LAYOUT_PREINITIALIZED;
            state->image->access[i] = static_cast<VkAccessFlagBits>(writable ? 0 : VK_ACCESS_MEMORY_WRITE_BIT);
            state->image->queue_family[i] = VK_QUEUE_FAMILY_IGNORED;
        }
        state->substituted = true;
        AVFrame* result = av_frame_alloc();
        if (!result) throw std::bad_alloc();
        result->format = AV_PIX_FMT_VULKAN; result->width = width; result->height = height;
        result->hw_frames_ctx = av_buffer_ref(state->allocation->hw_frames_ctx);
        result->data[0] = reinterpret_cast<uint8_t*>(state->image);
        result->buf[0] = av_buffer_create(result->data[0], sizeof(AVVkFrame),
            [](void* opaque, uint8_t*) { delete static_cast<NativeFrame*>(opaque); }, state.get(), 0);
        if (!result->hw_frames_ctx || !result->buf[0]) {
            // Do not let the AVBuffer callback race the still-owning unique_ptr.
            if (result->buf[0]) state.release();
            av_frame_free(&result);
            throw std::bad_alloc();
        }
        state.release();
        return result;
    }
}
}
uint64_t MacVulkanDeviceRegistryID(const AVFrame* frame) { auto* state = State(frame); return state ? state->registry_id : 0; }
int64_t MacVulkanFrameBytes(const AVFrame* frame) {
    auto* state = State(frame);
    if (!state) return 0;
    uint64_t size = CVPixelBufferGetDataSize(state->pixel);
    // The FFmpeg synchronization allocation is retained alongside the imported
    // surface. Include both, rather than letting GPU frames escape cache limits.
    for (int i = 0; i < state->count; ++i) {
        bool counted = false;
        for (int j = 0; j < i; ++j) counted |= state->original.mem[j] == state->original.mem[i];
        if (!counted) size += state->original.size[i];
    }
    return static_cast<int64_t>(std::min<uint64_t>(size, std::numeric_limits<int64_t>::max()));
}
bool IsMacVulkanFrame(const AVFrame* frame) { return State(frame) != nullptr; }
void WaitMacVulkanFrame(const AVFrame* frame) {
    auto* state = State(frame);
    if (!state) throw std::invalid_argument("Not a shared Mac Vulkan frame");
    state->Wait();
}
void* MacVulkanPixelBuffer(const AVFrame* frame) { auto* state = State(frame); return state ? state->pixel : nullptr; }
AVFrame* ImportVideoToolboxFrame(const AVFrame* frame, bool writable) {
    if (!frame || frame->format != AV_PIX_FMT_VIDEOTOOLBOX || !frame->data[3])
        throw std::invalid_argument("Expected a VideoToolbox frame");
    auto pixel = reinterpret_cast<CVPixelBufferRef>(frame->data[3]);
    const auto type = CVPixelBufferGetPixelFormatType(pixel);
    if (type != kCVPixelFormatType_420YpCbCr8BiPlanarVideoRange && type != kCVPixelFormatType_420YpCbCr8BiPlanarFullRange)
        throw std::runtime_error("Mac Vulkan currently supports 8-bit NV12 decode/encode surfaces");
    AVBufferRef* device = nullptr;
    AvCheck(CreateSharedVulkanDevice(&device), "Create Mac compositor device");
    AVFrame* result = nullptr;
    try { result = Wrap(device, pixel, true, writable); } catch (...) { av_buffer_unref(&device); throw; }
    av_buffer_unref(&device);
    const int props = av_frame_copy_props(result, frame);
    if (props < 0) { av_frame_free(&result); AvCheck(props, "Copy VideoToolbox metadata"); }
    return result;
}
AVFrame* AllocateMacVulkanFrame(AVBufferRef* device, int width, int height, bool nv12) {
    @autoreleasepool {
        NSDictionary* attrs = @{(id)kCVPixelBufferMetalCompatibilityKey: @YES, (id)kCVPixelBufferIOSurfacePropertiesKey: @{}};
        CVPixelBufferRef pixel = nullptr;
        const auto type = nv12 ? kCVPixelFormatType_420YpCbCr8BiPlanarVideoRange : kCVPixelFormatType_32BGRA;
        if (CVPixelBufferCreate(kCFAllocatorDefault, width, height, type, (CFDictionaryRef)attrs, &pixel) != kCVReturnSuccess)
            throw std::runtime_error("Allocate shared compositor IOSurface");
        AVFrame* frame = nullptr;
        try { frame = Wrap(device, pixel, nv12, true); } catch (...) { CVPixelBufferRelease(pixel); throw; }
        CVPixelBufferRelease(pixel);
        return frame;
    }
}
int ReadMacVulkanFrame(AVFrame* destination, const AVFrame* source) {
    auto* state = State(source);
    if (!state) return AVERROR(EINVAL);
    try { state->Wait(); } catch (...) { return AVERROR_EXTERNAL; }
    destination->format = state->fc->sw_format;
    destination->width = source->width; destination->height = source->height;
    int result = av_frame_get_buffer(destination, 32);
    if (result < 0) return result;
    // Managed Metal textures need an explicit GPU-to-CPU synchronization for
    // materialization. This is intentionally absent from preview and export.
    @autoreleasepool {
        auto first = state->Texture(0);
        id<MTLCommandQueue> queue = [first.device newCommandQueue];
        if (!queue) return AVERROR_EXTERNAL;
        id<MTLCommandBuffer> command = [queue commandBuffer];
        id<MTLBlitCommandEncoder> blit = [command blitCommandEncoder];
        for (int i = 0; i < state->count; ++i) {
            auto texture = state->Texture(i);
            if (texture.storageMode == MTLStorageModeManaged) [blit synchronizeResource:texture];
        }
        [blit endEncoding]; [command commit]; [command waitUntilCompleted];
        const bool completed = command.status == MTLCommandBufferStatusCompleted;
        [queue release];
        if (!completed) return AVERROR_EXTERNAL;
    }
    if (CVPixelBufferLockBaseAddress(state->pixel, kCVPixelBufferLock_ReadOnly) != kCVReturnSuccess) return AVERROR_EXTERNAL;
    const bool nv12 = destination->format == AV_PIX_FMT_NV12;
    const uint8_t* data[4]{}; int strides[4]{};
    for (int i = 0; i < (nv12 ? 2 : 1); ++i) {
        data[i] = static_cast<const uint8_t*>(nv12 ? CVPixelBufferGetBaseAddressOfPlane(state->pixel, i) : CVPixelBufferGetBaseAddress(state->pixel));
        strides[i] = static_cast<int>(nv12 ? CVPixelBufferGetBytesPerRowOfPlane(state->pixel, i) : CVPixelBufferGetBytesPerRow(state->pixel));
    }
    av_image_copy(destination->data, destination->linesize, data, strides, AVPixelFormat(destination->format), source->width, source->height);
    CVPixelBufferUnlockBaseAddress(state->pixel, kCVPixelBufferLock_ReadOnly);
    return 0;
}
void ReleaseMacVulkanPools() {
    auto& pools = NativePools(); std::lock_guard<std::mutex> lock(pools.mutex);
    for (auto& pair : pools.frames) av_buffer_unref(&pair.second);
    pools.frames.clear();
}
}
