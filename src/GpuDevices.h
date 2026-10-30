// Copyright (c) 2026 OpenShot Studios, LLC
// SPDX-License-Identifier: LGPL-3.0-or-later
#ifndef OPENSHOT_GPU_DEVICES_H
#define OPENSHOT_GPU_DEVICES_H
#include <string>
#include <vector>
#ifndef SWIG
struct AVBufferRef;
#endif
namespace openshot {
/// A Vulkan graphics card as OpenShot sees it.
struct GpuDevice {
    std::string name;
    std::string vendor;       ///< "NVIDIA", "AMD", "Intel", or the PCI vendor id
    std::string type;         ///< "discrete", "integrated", "virtual", "software" or "other"
    std::string driver;       ///< Driver name and version, when the driver reports them
    std::string vulkan_api;   ///< Vulkan API version advertised by the physical device
    std::string uuid;         ///< Stable id (hex), the same one CUDA reports for the card
    std::string render_node;  ///< Linux DRM render node such as /dev/dri/renderD128, if known
    bool video_decode = false;  ///< Has a Vulkan video decode queue
    bool video_encode = false;  ///< Has a Vulkan video encode queue (not codec-specific support)
    bool active = false;        ///< The card OpenShot renders with
};
/// Every Vulkan device on this system, including software ones. Empty without Vulkan.
std::vector<GpuDevice> ListGpuDevices();
/// The card OpenShot renders with, choosing it on first use. Empty name when
/// Vulkan is off (OPENSHOT_RENDER_BACKEND=cpu) or no usable GPU was found.
GpuDevice ActiveGpuDevice();
/// Hardware encoders (e.g. "h264_nvenc", "hevc_vaapi") that open on the active
/// card. Tested once per process with a tiny frame size; takes a fraction of a second.
std::vector<std::string> GpuEncoders();
#ifndef SWIG
/// The shared Vulkan device every reader decodes on, created on first use with the
/// best GPU (OPENSHOT_VULKAN_DEVICE, an index or a name, overrides it). Returns a new
/// reference, or a negative FFmpeg error when there is no usable GPU.
int CreateSharedVulkanDevice(AVBufferRef** output);
#endif
}
#endif
