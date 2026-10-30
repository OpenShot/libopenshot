// Copyright (c) 2026 OpenShot Studios, LLC
// SPDX-License-Identifier: LGPL-3.0-or-later
#ifndef OPENSHOT_MAC_VULKAN_INTEROP_H
#define OPENSHOT_MAC_VULKAN_INTEROP_H
#include <cstdint>
struct AVFrame;
struct AVBufferRef;
namespace openshot {
// Internal Apple bridge. Returned AVFrames are owned by the caller. Hardware
// surfaces retain their CVPixelBuffers until all Vulkan submissions have retired.
AVFrame* ImportVideoToolboxFrame(const AVFrame* frame, bool writable = false);
AVFrame* AllocateMacVulkanFrame(AVBufferRef* device, int width, int height, bool nv12);
uint64_t MacVulkanDeviceRegistryID(const AVFrame* frame);
int64_t MacVulkanFrameBytes(const AVFrame* frame);
bool IsMacVulkanFrame(const AVFrame* frame);
void WaitMacVulkanFrame(const AVFrame* frame);
// Borrowed CVPixelBufferRef; valid while the AVFrame is retained.
void* MacVulkanPixelBuffer(const AVFrame* frame);
// Explicit CPU fallback only. Returns an FFmpeg status; never used by GPU export.
int ReadMacVulkanFrame(AVFrame* destination, const AVFrame* source);
void ReleaseMacVulkanPools();
}
#endif
