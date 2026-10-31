// Copyright (c) 2026 OpenShot Studios, LLC
// SPDX-License-Identifier: LGPL-3.0-or-later
#include "GpuDevices.h"
#include "Logger.h"
#include "VulkanFrameImage.h"
#include <algorithm>
#include <cerrno>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <mutex>
#include <utility>
#include <stdexcept>
extern "C" {
#include <libavcodec/avcodec.h>
#include <libavutil/hwcontext.h>
#include <libavutil/hwcontext_vulkan.h>
#include <libavutil/error.h>
}
namespace openshot {
namespace {
// A temporary instance for listing devices; FFmpeg keeps its own for rendering.
struct ListingInstance {
    VkInstance instance = VK_NULL_HANDLE;
    ListingInstance() {
        VkApplicationInfo app = {};
        app.sType = VK_STRUCTURE_TYPE_APPLICATION_INFO;
        app.apiVersion = VK_API_VERSION_1_1;
        VkInstanceCreateInfo create = {};
        create.sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO;
        create.pApplicationInfo = &app;
        if (vkCreateInstance(&create, nullptr, &instance) != VK_SUCCESS)
            instance = VK_NULL_HANDLE;
    }
    ~ListingInstance() { if (instance) vkDestroyInstance(instance, nullptr); }
    std::vector<VkPhysicalDevice> Devices() const {
        uint32_t count = 0;
        if (!instance) return {};
        vkEnumeratePhysicalDevices(instance, &count, nullptr);
        std::vector<VkPhysicalDevice> devices(count);
        vkEnumeratePhysicalDevices(instance, &count, devices.data());
        return devices;
    }
};

bool HasExtension(VkPhysicalDevice device, const char* name) {
    uint32_t count = 0;
    vkEnumerateDeviceExtensionProperties(device, nullptr, &count, nullptr);
    std::vector<VkExtensionProperties> extensions(count);
    vkEnumerateDeviceExtensionProperties(device, nullptr, &count, extensions.data());
    return std::any_of(extensions.begin(), extensions.end(),
        [name](const VkExtensionProperties& e) { return std::strcmp(e.extensionName, name) == 0; });
}

GpuDevice Describe(VkPhysicalDevice device) {
    VkPhysicalDeviceProperties basic;
    vkGetPhysicalDeviceProperties(device, &basic);
    VkPhysicalDeviceIDProperties ids = {};
    ids.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_ID_PROPERTIES;
    VkPhysicalDeviceDriverProperties driver = {};
    driver.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_DRIVER_PROPERTIES;
    VkPhysicalDeviceDrmPropertiesEXT drm = {};
    drm.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_DRM_PROPERTIES_EXT;
    VkPhysicalDeviceProperties2 props = {};
    props.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_PROPERTIES_2;
    props.pNext = &ids;
    // Chain only the structures this device understands.
    void** next = &ids.pNext;
    const bool has_driver = basic.apiVersion >= VK_API_VERSION_1_2;
    if (has_driver) { *next = &driver; next = &driver.pNext; }
    const bool has_drm = HasExtension(device, VK_EXT_PHYSICAL_DEVICE_DRM_EXTENSION_NAME);
    if (has_drm) { *next = &drm; next = &drm.pNext; }
    vkGetPhysicalDeviceProperties2(device, &props);

    GpuDevice gpu;
    gpu.name = basic.deviceName;
    gpu.vulkan_api = std::to_string(VK_API_VERSION_MAJOR(basic.apiVersion)) + "." +
        std::to_string(VK_API_VERSION_MINOR(basic.apiVersion)) + "." +
        std::to_string(VK_API_VERSION_PATCH(basic.apiVersion));
    switch (basic.vendorID) {
        case 0x10DE: gpu.vendor = "NVIDIA"; break;
        case 0x1002: gpu.vendor = "AMD"; break;
        case 0x8086: gpu.vendor = "Intel"; break;
        default: {
            char id[16];
            std::snprintf(id, sizeof(id), "0x%04x", basic.vendorID);
            gpu.vendor = id;
        }
    }
    switch (basic.deviceType) {
        case VK_PHYSICAL_DEVICE_TYPE_DISCRETE_GPU: gpu.type = "discrete"; break;
        case VK_PHYSICAL_DEVICE_TYPE_INTEGRATED_GPU: gpu.type = "integrated"; break;
        case VK_PHYSICAL_DEVICE_TYPE_VIRTUAL_GPU: gpu.type = "virtual"; break;
        case VK_PHYSICAL_DEVICE_TYPE_CPU: gpu.type = "software"; break;
        default: gpu.type = "other";
    }
    if (has_driver)
        gpu.driver = std::string(driver.driverName) + " " + driver.driverInfo;
    for (uint8_t byte : ids.deviceUUID) {
        char hex[3];
        std::snprintf(hex, sizeof(hex), "%02x", byte);
        gpu.uuid += hex;
    }
    if (has_drm && drm.hasRender)
        gpu.render_node = "/dev/dri/renderD" + std::to_string(drm.renderMinor);

    uint32_t families = 0;
    vkGetPhysicalDeviceQueueFamilyProperties(device, &families, nullptr);
    std::vector<VkQueueFamilyProperties> queues(families);
    vkGetPhysicalDeviceQueueFamilyProperties(device, &families, queues.data());
    gpu.video_decode = std::any_of(queues.begin(), queues.end(),
        [](const VkQueueFamilyProperties& q) { return q.queueFlags & VK_QUEUE_VIDEO_DECODE_BIT_KHR; });
    gpu.video_encode = std::any_of(queues.begin(), queues.end(),
        [](const VkQueueFamilyProperties& q) { return q.queueFlags & VK_QUEUE_VIDEO_ENCODE_BIT_KHR; });
    return gpu;
}

// FFmpeg uses the first Vulkan device, which can be a software driver such as
// llvmpipe. List real GPUs instead, discrete before integrated. Software (CPU)
// devices are skipped: they cannot decode video and are slower than our CPU path.
std::vector<std::string> VulkanDeviceCandidates() {
    const char* chosen = std::getenv("OPENSHOT_VULKAN_DEVICE");
    if (chosen && chosen[0])
        return {chosen};
    ListingInstance listing;
    if (!listing.instance)
        return {""};  // Let FFmpeg report why Vulkan is unavailable
    std::vector<std::pair<int, std::string>> ranked;
    for (VkPhysicalDevice device : listing.Devices()) {
        VkPhysicalDeviceProperties props;
        vkGetPhysicalDeviceProperties(device, &props);
        if (props.deviceType == VK_PHYSICAL_DEVICE_TYPE_CPU)
            continue;
        const int rank = props.deviceType == VK_PHYSICAL_DEVICE_TYPE_DISCRETE_GPU ? 0
            : props.deviceType == VK_PHYSICAL_DEVICE_TYPE_INTEGRATED_GPU ? 1 : 2;
        // Select by name: FFmpeg's device indices can be in a different order
        ranked.emplace_back(rank, props.deviceName);
    }
    std::stable_sort(ranked.begin(), ranked.end(),
        [](const auto& a, const auto& b) { return a.first < b.first; });
    std::vector<std::string> candidates;
    for (const auto& device : ranked)
        candidates.push_back(device.second);
    return candidates;
}

VkPhysicalDevice PhysicalDevice(AVBufferRef* device) {
    auto* context = reinterpret_cast<AVHWDeviceContext*>(device->data);
    return static_cast<AVVulkanDeviceContext*>(context->hwctx)->phys_dev;
}

// Try opening an encoder at a tiny size on the given hardware device.
bool EncoderOpens(const char* name, AVBufferRef* device, AVPixelFormat hw_format) {
    const AVCodec* codec = avcodec_find_encoder_by_name(name);
    if (!codec) {
        Logger::Instance()->Log(std::string("GPU encoder probe: ") + name + " is not compiled into FFmpeg", Logger::LevelDebug);
        return false;
    }
    AVCodecContext* context = avcodec_alloc_context3(codec);
    if (!context) return false;
    context->width = 640;
    context->height = 360;
    context->time_base = {1, 30};
    context->framerate = {30, 1};
    context->bit_rate = 1000000;
    bool opened = false;
    if (hw_format == AV_PIX_FMT_VAAPI || hw_format == AV_PIX_FMT_VULKAN || hw_format == AV_PIX_FMT_VIDEOTOOLBOX) {
        AVBufferRef* frames = av_hwframe_ctx_alloc(device);
        if (frames) {
            auto* fc = reinterpret_cast<AVHWFramesContext*>(frames->data);
            fc->format = hw_format;
            fc->sw_format = AV_PIX_FMT_NV12;
            fc->width = context->width;
            fc->height = context->height;
            if (av_hwframe_ctx_init(frames) >= 0) {
                context->pix_fmt = hw_format;
                context->hw_frames_ctx = av_buffer_ref(frames);
            }
            av_buffer_unref(&frames);
        }
    } else {
        // NVENC takes system-memory frames on the CUDA device given here
        context->pix_fmt = AV_PIX_FMT_NV12;
        context->hw_device_ctx = av_buffer_ref(device);
    }
    if (context->hw_frames_ctx || context->hw_device_ctx) {
        const int result = avcodec_open2(context, codec, nullptr);
        opened = result >= 0;
        if (result < 0) {
            char message[AV_ERROR_MAX_STRING_SIZE];
            av_strerror(result, message, sizeof(message));
            Logger::Instance()->Log(std::string("GPU encoder probe failed: ") + name + ": " + message, Logger::LevelDebug);
        }
    }
    avcodec_free_context(&context);
    return opened;
}
}  // namespace

int CreateSharedVulkanDevice(AVBufferRef** output) {
    // All readers share one device so their surfaces can be composed without interop.
    // Keep an owning reference for the process lifetime; each caller gets its own ref.
    struct SharedDevice {
        std::mutex mutex;
        AVBufferRef* device = nullptr;
        bool tried = false;
        ~SharedDevice() { av_buffer_unref(&device); }
    };
    static SharedDevice shared;
    const std::lock_guard<std::mutex> lock(shared.mutex);
    if (!shared.device) {
        // Only search once; without a usable GPU every reader decodes on the CPU.
        if (shared.tried)
            return AVERROR(ENODEV);
        shared.tried = true;
#ifdef __APPLE__
        // Work around imported-texture argument-buffer writes on older Intel
        // Metal drivers. An explicit user setting still takes precedence.
        setenv("MVK_CONFIG_USE_METAL_ARGUMENT_BUFFERS", "0", 0);
#endif
        int result = AVERROR(ENODEV);
        for (const auto& device : VulkanDeviceCandidates()) {
            Logger::Instance()->Log("Vulkan device initialization: " + (device.empty() ? std::string("default") : device), Logger::LevelInfo);
            AVDictionary* options = nullptr;
#ifdef __APPLE__
            av_dict_set(&options, "device_extensions", "VK_EXT_metal_objects", 0);
#endif
            result = av_hwdevice_ctx_create(&shared.device, AV_HWDEVICE_TYPE_VULKAN,
                device.empty() ? nullptr : device.c_str(), options, 0);
            av_dict_free(&options);
            if (result >= 0)
                break;
            char message[AV_ERROR_MAX_STRING_SIZE];
            av_strerror(result, message, sizeof(message));
            Logger::Instance()->Log("Vulkan device initialization failed: " + device + ": " + message +
                                   " (" + std::to_string(result) + ")", Logger::LevelWarning);
        }
        if (result < 0) {
            Logger::Instance()->Log("No usable Vulkan GPU; decoding on the CPU", Logger::LevelWarning);
            return result;
        }
        VkPhysicalDeviceProperties props;
        vkGetPhysicalDeviceProperties(PhysicalDevice(shared.device), &props);
        Logger::Instance()->Log(std::string("Vulkan device: ") + props.deviceName, Logger::LevelInfo);
    }
    *output = av_buffer_ref(shared.device);
    return *output ? 0 : AVERROR(ENOMEM);
}

GpuDevice ActiveGpuDevice() {
    AVBufferRef* device = nullptr;
    if (!VulkanRenderingEnabled() || CreateSharedVulkanDevice(&device) < 0)
        return {};
    GpuDevice gpu = Describe(PhysicalDevice(device));
    gpu.active = true;
    av_buffer_unref(&device);
    return gpu;
}

void ConfigureGpuEnvironment(const std::string& backend, const std::string& device) {
#ifdef _WIN32
    if (_putenv_s("OPENSHOT_RENDER_BACKEND", backend.c_str()) != 0 ||
        _putenv_s("OPENSHOT_VULKAN_DEVICE", device.c_str()) != 0)
        throw std::runtime_error("Unable to apply native GPU environment");
#else
    const auto apply = [](const char* key, const std::string& value) {
        return value.empty() ? unsetenv(key) : setenv(key, value.c_str(), 1);
    };
    if (apply("OPENSHOT_RENDER_BACKEND", backend) != 0 || apply("OPENSHOT_VULKAN_DEVICE", device) != 0)
        throw std::runtime_error("Unable to apply native GPU environment");
#endif
}

std::vector<GpuDevice> ListGpuDevices() {
    const std::string active = ActiveGpuDevice().uuid;
    ListingInstance listing;
    std::vector<GpuDevice> devices;
    for (VkPhysicalDevice device : listing.Devices()) {
        devices.push_back(Describe(device));
        devices.back().active = !active.empty() && devices.back().uuid == active;
    }
    return devices;
}

std::vector<std::string> GpuEncoders() {
    static std::mutex mutex;
    static bool tested = false;
    static std::vector<std::string> encoders;
    const std::lock_guard<std::mutex> lock(mutex);
    if (tested)
        return encoders;
    tested = true;
    AVBufferRef* vulkan = nullptr;
    if (!VulkanRenderingEnabled() || CreateSharedVulkanDevice(&vulkan) < 0)
        return encoders;
    const GpuDevice gpu = Describe(PhysicalDevice(vulkan));
    // Encoders that cannot open print errors; this is only a capability test.
    const int log_level = av_log_get_level();
    av_log_set_level(AV_LOG_QUIET);
    // Prefer the same Vulkan device/API for every vendor, when its driver and
    // this FFmpeg build support the requested codec. Probe, never assume.
    // Old loaders (1.2.131 on Ubuntu 20.04) return a stub without a driver
    // function for the encode queries, and FFmpeg crashes calling it.
    // 1.3.270 is the oldest loader verified to fail cleanly instead.
    uint32_t loader_version = 0;
    if (vkEnumerateInstanceVersion(&loader_version) == VK_SUCCESS &&
        loader_version >= VK_MAKE_API_VERSION(0, 1, 3, 270)) {
        for (const char* name : {"h264_vulkan", "hevc_vulkan", "av1_vulkan"})
            if (EncoderOpens(name, vulkan, AV_PIX_FMT_VULKAN)) encoders.push_back(name);
    }
    // CUDA derived from Vulkan is the same card (matched by UUID)
    AVBufferRef* cuda = nullptr;
    if (av_hwdevice_ctx_create_derived(&cuda, AV_HWDEVICE_TYPE_CUDA, vulkan, 0) >= 0) {
        for (const char* name : {"h264_nvenc", "hevc_nvenc", "av1_nvenc"})
            if (EncoderOpens(name, cuda, AV_PIX_FMT_CUDA)) encoders.push_back(name);
        av_buffer_unref(&cuda);
    }
#ifdef __APPLE__
    AVBufferRef* videotoolbox = nullptr;
    if (av_hwdevice_ctx_create(&videotoolbox, AV_HWDEVICE_TYPE_VIDEOTOOLBOX, nullptr, nullptr, 0) >= 0) {
        for (const char* name : {"h264_videotoolbox", "hevc_videotoolbox"})
            if (EncoderOpens(name, videotoolbox, AV_PIX_FMT_VIDEOTOOLBOX)) encoders.push_back(name);
        av_buffer_unref(&videotoolbox);
    }
#endif
    AVBufferRef* vaapi = nullptr;
    if (!gpu.render_node.empty() &&
        av_hwdevice_ctx_create(&vaapi, AV_HWDEVICE_TYPE_VAAPI, gpu.render_node.c_str(), nullptr, 0) >= 0) {
        for (const char* name : {"h264_vaapi", "hevc_vaapi", "av1_vaapi", "vp9_vaapi"})
            if (EncoderOpens(name, vaapi, AV_PIX_FMT_VAAPI)) encoders.push_back(name);
        av_buffer_unref(&vaapi);
    }
    av_log_set_level(log_level);
    av_buffer_unref(&vulkan);
    std::string list;
    for (const auto& name : encoders)
        list += (list.empty() ? "" : ", ") + name;
    Logger::Instance()->Log("GPU encoders on " + gpu.name + ": " + (list.empty() ? "none" : list), Logger::LevelInfo);
    return encoders;
}
}
