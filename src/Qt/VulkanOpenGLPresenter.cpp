// Copyright (c) 2026 OpenShot Studios, LLC
// SPDX-License-Identifier: LGPL-3.0-or-later
#include "VulkanOpenGLPresenter.h"
#include "VideoRenderer.h"
#include "../VulkanFrameImage.h"
#include "../VulkanCompositor.h"
#include "../Logger.h"
#include <QOpenGLContext>
#include <QOpenGLFunctions>
#include <QOpenGLExtraFunctions>
#include <QOpenGLShaderProgram>
#include <QPointer>
#include <QWidget>
#include <QVariant>
#include <QDebug>
#include <QByteArray>
#include <QThread>
#include "ExternalOpenGL.h"
#ifdef _WIN32
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#define VK_USE_PLATFORM_WIN32_KHR
#else
#include <unistd.h>
#endif
#include <stdexcept>
#include <cstring>
#include <chrono>
extern "C" {
#include <libavutil/hwcontext_vulkan.h>
}

namespace openshot {
namespace {
void check(VkResult result, const char* operation) {
    if (result != VK_SUCCESS) throw std::runtime_error(operation);
}
template<class T> T glFunction(QOpenGLContext* context, const char* name) {
    auto function = reinterpret_cast<T>(context->getProcAddress(name));
    if (!function) throw std::runtime_error(name);
    return function;
}
#ifdef _WIN32
constexpr auto kMemoryHandle = VK_EXTERNAL_MEMORY_HANDLE_TYPE_OPAQUE_WIN32_BIT;
constexpr auto kSemaphoreHandle = VK_EXTERNAL_SEMAPHORE_HANDLE_TYPE_OPAQUE_WIN32_BIT;
constexpr GLenum kGLHandle = GL_HANDLE_TYPE_OPAQUE_WIN32_EXT;
constexpr const char* kMemoryExtension = "GL_EXT_memory_object_win32";
constexpr const char* kSemaphoreExtension = "GL_EXT_semaphore_win32";
using ImportMemory = PFNGLIMPORTMEMORYWIN32HANDLEEXTPROC;
using ImportSemaphore = PFNGLIMPORTSEMAPHOREWIN32HANDLEEXTPROC;
#else
constexpr auto kMemoryHandle = VK_EXTERNAL_MEMORY_HANDLE_TYPE_OPAQUE_FD_BIT;
constexpr auto kSemaphoreHandle = VK_EXTERNAL_SEMAPHORE_HANDLE_TYPE_OPAQUE_FD_BIT;
constexpr GLenum kGLHandle = GL_HANDLE_TYPE_OPAQUE_FD_EXT;
constexpr const char* kMemoryExtension = "GL_EXT_memory_object_fd";
constexpr const char* kSemaphoreExtension = "GL_EXT_semaphore_fd";
using ImportMemory = PFNGLIMPORTMEMORYFDEXTPROC;
using ImportSemaphore = PFNGLIMPORTSEMAPHOREFDEXTPROC;
#endif
constexpr VkImageSubresourceRange kColor = {VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1};
}
// Frames reach OpenGL through a presenter-owned linear buffer, never through
// shared image memory. NVIDIA's Windows driver returns zeros when FFmpeg or
// compositor frames are read through an exported alias of their memory, and
// Maxwell GPUs raise a TEX LAYOUT exception (then a TDR) when OpenGL samples
// an imported optimal-tiling Vulkan image. Vulkan copies each frame into the
// buffer, and OpenGL unpacks it into its own texture. The buffer and its
// OpenGL import persist until the frame size, device, or context changes.
struct SharedTarget {
    AVBufferRef* device_ref = nullptr;
    VkDevice device = VK_NULL_HANDLE;
    QPointer<QOpenGLContext> context;
    int width = 0, height = 0;
    uint32_t family = 0;
    VkQueue queue = VK_NULL_HANDLE;
    VkBuffer pixels = VK_NULL_HANDLE;
    VkDeviceMemory memory = VK_NULL_HANDLE;
    // Separate directions: reusing one binary semaphore lets Vulkan's
    // acquire consume its own release signal before GL can wait on it.
    VkSemaphore handoff[2] = {VK_NULL_HANDLE, VK_NULL_HANDLE};
    VkCommandPool pool = VK_NULL_HANDLE;
    VkCommandBuffer commands[2] = {VK_NULL_HANDLE, VK_NULL_HANDLE};
    VkFence fence = VK_NULL_HANDLE;
    bool pending = false;
    GLuint gl_memory = 0, gl_pixels = 0, texture = 0, gl_semaphores[2] = {0, 0}, buffer = 0, vertex_array = 0;
    std::unique_ptr<QOpenGLShaderProgram> program;
    PFNGLWAITSEMAPHOREEXTPROC wait_semaphore = nullptr;
    PFNGLSIGNALSEMAPHOREEXTPROC signal_semaphore = nullptr;
};
struct VulkanOpenGLPresenter::Impl : QObject, FramePresenter {
    QPointer<QWidget> widget;
    std::shared_ptr<Frame> frame;
    std::shared_ptr<const VulkanFrameImage> display_surface;
    bool fallback_delivered = false;
    uint64_t native_draws = 0;
    uint64_t fallback_frames = 0;
    std::string last_failure;
    QPointer<QOpenGLContext> logged_context;
    bool identity_logged = false;
    SharedTarget target;
    bool unavailable(const std::string& reason) { last_failure = reason; return false; }
    explicit Impl(QWidget* value) : widget(value) {
        value->setProperty("openshot_gpu_presenter", QVariant::fromValue(static_cast<QObject*>(this)));
    }
    ~Impl() override {
        releaseTarget();
        if (widget) widget->setProperty("openshot_gpu_presenter", QVariant());
    }
    void PresentFrame(const std::shared_ptr<Frame>& value) override {
        frame = value;
        display_surface.reset();
        fallback_delivered = false;
        if (frame && !frame->GetDeferredImage()) {
            last_failure = "Frame is CPU-backed (decode, rendering, or an effect required CPU pixels)";
            fallback();
        }
        if (widget) {
            if (frame && frame->GetDeferredImage() && widget->metaObject()->indexOfMethod("gpu_presented(int)") >= 0)
                QMetaObject::invokeMethod(widget, "gpu_presented", Qt::DirectConnection,
                                          Q_ARG(int, static_cast<int>(frame->number)));
            widget->update();
        }
    }
    void fallback() {
        if (!fallback_delivered && frame && widget) {
            fallback_delivered = true;
            ++fallback_frames;
            display_surface.reset();
            const QImage image = *frame->GetImage();
            QMetaObject::invokeMethod(widget, "present", Qt::QueuedConnection, Q_ARG(QImage, image));
        }
    }
    void waitPending() {
        if (!target.pending) return;
        target.pending = false;
        check(vkWaitForFences(target.device, 1, &target.fence, VK_TRUE, UINT64_MAX), "Wait for presentation transfer");
        check(vkResetFences(target.device, 1, &target.fence), "Reset presentation fence");
    }
    // OpenGL objects are deleted only while their context is current; otherwise
    // they are released with that context.
    void releaseTarget() {
        if (target.device) {
            try { waitPending(); } catch (const std::exception&) { vkDeviceWaitIdle(target.device); }
        }
        auto* context = QOpenGLContext::currentContext();
        if (target.context && context == target.context) {
            auto gl = context->functions();
            if (target.vertex_array) context->extraFunctions()->glDeleteVertexArrays(1, &target.vertex_array);
            if (target.buffer) gl->glDeleteBuffers(1, &target.buffer);
            if (target.gl_pixels) gl->glDeleteBuffers(1, &target.gl_pixels);
            if (target.texture) gl->glDeleteTextures(1, &target.texture);
            if (target.gl_memory)
                reinterpret_cast<PFNGLDELETEMEMORYOBJECTSEXTPROC>(context->getProcAddress("glDeleteMemoryObjectsEXT"))(1, &target.gl_memory);
            if (target.gl_semaphores[0] || target.gl_semaphores[1])
                reinterpret_cast<PFNGLDELETESEMAPHORESEXTPROC>(context->getProcAddress("glDeleteSemaphoresEXT"))(2, target.gl_semaphores);
        }
        target.program.reset(); // Qt defers deletion until the context is current.
        if (target.device) {
            if (target.fence) vkDestroyFence(target.device, target.fence, nullptr);
            if (target.pool) vkDestroyCommandPool(target.device, target.pool, nullptr);
            for (auto semaphore : target.handoff)
                if (semaphore) vkDestroySemaphore(target.device, semaphore, nullptr);
            if (target.pixels) vkDestroyBuffer(target.device, target.pixels, nullptr);
            if (target.memory) vkFreeMemory(target.device, target.memory, nullptr);
        }
        av_buffer_unref(&target.device_ref);
        target = SharedTarget();
    }
    void createTarget(QOpenGLContext* context, const AVHWFramesContext* fc, int width, int height) {
        auto vk = static_cast<AVVulkanDeviceContext*>(fc->device_ctx->hwctx);
        target.device_ref = av_buffer_ref(fc->device_ref);
        if (!target.device_ref) throw std::bad_alloc();
        target.device = vk->act_dev;
        target.context = context;
        target.width = width;
        target.height = height;
        target.family = vk->queue_family_comp_index;
        vkGetDeviceQueue(target.device, target.family, 0, &target.queue);
        auto gl = context->functions();
        auto extra = context->extraFunctions();
        auto createMemory = glFunction<PFNGLCREATEMEMORYOBJECTSEXTPROC>(context, "glCreateMemoryObjectsEXT");
        auto memoryParameter = glFunction<PFNGLMEMORYOBJECTPARAMETERIVEXTPROC>(context, "glMemoryObjectParameterivEXT");
        auto bufferMemory = glFunction<PFNGLBUFFERSTORAGEMEMEXTPROC>(context, "glBufferStorageMemEXT");
        auto createSemaphores = glFunction<PFNGLGENSEMAPHORESEXTPROC>(context, "glGenSemaphoresEXT");
        glFunction<PFNGLDELETEMEMORYOBJECTSEXTPROC>(context, "glDeleteMemoryObjectsEXT");
        glFunction<PFNGLDELETESEMAPHORESEXTPROC>(context, "glDeleteSemaphoresEXT");
        target.wait_semaphore = glFunction<PFNGLWAITSEMAPHOREEXTPROC>(context, "glWaitSemaphoreEXT");
        target.signal_semaphore = glFunction<PFNGLSIGNALSEMAPHOREEXTPROC>(context, "glSignalSemaphoreEXT");
#ifdef _WIN32
        auto importMemory = glFunction<ImportMemory>(context, "glImportMemoryWin32HandleEXT");
        auto importSemaphore = glFunction<ImportSemaphore>(context, "glImportSemaphoreWin32HandleEXT");
        auto getMemoryHandle = reinterpret_cast<PFN_vkGetMemoryWin32HandleKHR>(vkGetDeviceProcAddr(target.device, "vkGetMemoryWin32HandleKHR"));
        auto getSemaphoreHandle = reinterpret_cast<PFN_vkGetSemaphoreWin32HandleKHR>(vkGetDeviceProcAddr(target.device, "vkGetSemaphoreWin32HandleKHR"));
#else
        auto importMemory = glFunction<ImportMemory>(context, "glImportMemoryFdEXT");
        auto importSemaphore = glFunction<ImportSemaphore>(context, "glImportSemaphoreFdEXT");
        auto getMemoryHandle = reinterpret_cast<PFN_vkGetMemoryFdKHR>(vkGetDeviceProcAddr(target.device, "vkGetMemoryFdKHR"));
        auto getSemaphoreHandle = reinterpret_cast<PFN_vkGetSemaphoreFdKHR>(vkGetDeviceProcAddr(target.device, "vkGetSemaphoreFdKHR"));
#endif
        if (!getMemoryHandle || !getSemaphoreHandle)
            throw std::runtime_error("Vulkan device lacks external memory/semaphore export");

        const VkDeviceSize size = VkDeviceSize(width) * height * 4;
        VkExternalMemoryBufferCreateInfo external{VK_STRUCTURE_TYPE_EXTERNAL_MEMORY_BUFFER_CREATE_INFO};
        external.handleTypes = kMemoryHandle;
        VkBufferCreateInfo create{VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO, &external};
        create.size = size;
        create.usage = VK_BUFFER_USAGE_TRANSFER_DST_BIT;
        create.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
        check(vkCreateBuffer(target.device, &create, nullptr, &target.pixels), "Create presentation buffer");
        VkMemoryDedicatedRequirements dedicated{VK_STRUCTURE_TYPE_MEMORY_DEDICATED_REQUIREMENTS};
        VkMemoryRequirements2 requirements{VK_STRUCTURE_TYPE_MEMORY_REQUIREMENTS_2, &dedicated};
        VkBufferMemoryRequirementsInfo2 requirementInfo{VK_STRUCTURE_TYPE_BUFFER_MEMORY_REQUIREMENTS_INFO_2};
        requirementInfo.buffer = target.pixels;
        vkGetBufferMemoryRequirements2(target.device, &requirementInfo, &requirements);
        VkPhysicalDeviceMemoryProperties memoryProperties;
        vkGetPhysicalDeviceMemoryProperties(vk->phys_dev, &memoryProperties);
        uint32_t memoryType = UINT32_MAX;
        for (uint32_t i = 0; i < memoryProperties.memoryTypeCount && memoryType == UINT32_MAX; ++i)
            if ((requirements.memoryRequirements.memoryTypeBits & (1u << i)) &&
                (memoryProperties.memoryTypes[i].propertyFlags & VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT))
                memoryType = i;
        if (memoryType == UINT32_MAX) throw std::runtime_error("No device-local presentation memory");
        const bool useDedicated = dedicated.prefersDedicatedAllocation || dedicated.requiresDedicatedAllocation;
        VkMemoryDedicatedAllocateInfo dedicatedInfo{VK_STRUCTURE_TYPE_MEMORY_DEDICATED_ALLOCATE_INFO};
        dedicatedInfo.buffer = target.pixels;
        VkExportMemoryAllocateInfo exportInfo{VK_STRUCTURE_TYPE_EXPORT_MEMORY_ALLOCATE_INFO, useDedicated ? &dedicatedInfo : nullptr};
        exportInfo.handleTypes = kMemoryHandle;
        VkMemoryAllocateInfo allocate{VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO, &exportInfo};
        allocate.allocationSize = requirements.memoryRequirements.size;
        allocate.memoryTypeIndex = memoryType;
        check(vkAllocateMemory(target.device, &allocate, nullptr, &target.memory), "Allocate presentation memory");
        check(vkBindBufferMemory(target.device, target.pixels, target.memory, 0), "Bind presentation memory");

        createMemory(1, &target.gl_memory);
        GLint glDedicated = useDedicated;
        memoryParameter(target.gl_memory, GL_DEDICATED_MEMORY_OBJECT_EXT, &glDedicated);
#ifdef _WIN32
        VkMemoryGetWin32HandleInfoKHR memoryInfo{VK_STRUCTURE_TYPE_MEMORY_GET_WIN32_HANDLE_INFO_KHR};
        memoryInfo.memory = target.memory;
        memoryInfo.handleType = kMemoryHandle;
        HANDLE memoryHandle = nullptr;
        check(getMemoryHandle(target.device, &memoryInfo, &memoryHandle), "Export presentation memory");
        importMemory(target.gl_memory, requirements.memoryRequirements.size, kGLHandle, memoryHandle);
        // Win32 imports retain the resource, not ownership of the handle.
        CloseHandle(memoryHandle);
#else
        VkMemoryGetFdInfoKHR memoryInfo{VK_STRUCTURE_TYPE_MEMORY_GET_FD_INFO_KHR};
        memoryInfo.memory = target.memory;
        memoryInfo.handleType = kMemoryHandle;
        int memoryFd = -1;
        check(getMemoryHandle(target.device, &memoryInfo, &memoryFd), "Export presentation memory");
        importMemory(target.gl_memory, requirements.memoryRequirements.size, kGLHandle, memoryFd); // GL owns the fd.
#endif
        GLint previousUnpack = 0, previousTexture = 0;
        gl->glGetIntegerv(GL_PIXEL_UNPACK_BUFFER_BINDING, &previousUnpack);
        gl->glGetIntegerv(GL_TEXTURE_BINDING_2D, &previousTexture);
        gl->glGenBuffers(1, &target.gl_pixels);
        gl->glBindBuffer(GL_PIXEL_UNPACK_BUFFER, target.gl_pixels);
        bufferMemory(GL_PIXEL_UNPACK_BUFFER, size, target.gl_memory, 0);
        gl->glBindBuffer(GL_PIXEL_UNPACK_BUFFER, previousUnpack);
        gl->glGenTextures(1, &target.texture);
        gl->glBindTexture(GL_TEXTURE_2D, target.texture);
        gl->glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
        gl->glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
        gl->glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
        gl->glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
        gl->glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, width, height, 0, GL_RGBA, GL_UNSIGNED_BYTE, nullptr);
        gl->glBindTexture(GL_TEXTURE_2D, previousTexture);

        VkExportSemaphoreCreateInfo exportSemaphore{VK_STRUCTURE_TYPE_EXPORT_SEMAPHORE_CREATE_INFO};
        exportSemaphore.handleTypes = kSemaphoreHandle;
        VkSemaphoreCreateInfo semaphoreInfo{VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO, &exportSemaphore};
        createSemaphores(2, target.gl_semaphores);
        for (int direction = 0; direction < 2; ++direction) {
            check(vkCreateSemaphore(target.device, &semaphoreInfo, nullptr, &target.handoff[direction]), "Create presentation semaphore");
#ifdef _WIN32
            VkSemaphoreGetWin32HandleInfoKHR handleInfo{VK_STRUCTURE_TYPE_SEMAPHORE_GET_WIN32_HANDLE_INFO_KHR};
            handleInfo.semaphore = target.handoff[direction];
            handleInfo.handleType = kSemaphoreHandle;
            HANDLE semaphoreHandle = nullptr;
            check(getSemaphoreHandle(target.device, &handleInfo, &semaphoreHandle), "Export presentation semaphore");
            importSemaphore(target.gl_semaphores[direction], kGLHandle, semaphoreHandle);
            CloseHandle(semaphoreHandle);
#else
            VkSemaphoreGetFdInfoKHR handleInfo{VK_STRUCTURE_TYPE_SEMAPHORE_GET_FD_INFO_KHR};
            handleInfo.semaphore = target.handoff[direction];
            handleInfo.handleType = kSemaphoreHandle;
            int semaphoreFd = -1;
            check(getSemaphoreHandle(target.device, &handleInfo, &semaphoreFd), "Export presentation semaphore");
            importSemaphore(target.gl_semaphores[direction], kGLHandle, semaphoreFd);
#endif
        }
        if (gl->glGetError() != GL_NO_ERROR) throw std::runtime_error("OpenGL external memory import failed");

        VkCommandPoolCreateInfo poolInfo{VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO};
        poolInfo.flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT;
        poolInfo.queueFamilyIndex = target.family;
        check(vkCreateCommandPool(target.device, &poolInfo, nullptr, &target.pool), "Create presentation command pool");
        VkCommandBufferAllocateInfo allocation{VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO};
        allocation.commandPool = target.pool;
        allocation.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
        allocation.commandBufferCount = 2;
        check(vkAllocateCommandBuffers(target.device, &allocation, target.commands), "Allocate presentation commands");
        VkFenceCreateInfo fenceInfo{VK_STRUCTURE_TYPE_FENCE_CREATE_INFO};
        check(vkCreateFence(target.device, &fenceInfo, nullptr, &target.fence), "Create presentation fence");
        // The acquire from OpenGL is the same every frame.
        VkCommandBufferBeginInfo begin{VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO};
        check(vkBeginCommandBuffer(target.commands[1], &begin), "Begin presentation acquire");
        VkBufferMemoryBarrier acquire{VK_STRUCTURE_TYPE_BUFFER_MEMORY_BARRIER};
        acquire.dstAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
        acquire.srcQueueFamilyIndex = VK_QUEUE_FAMILY_EXTERNAL;
        acquire.dstQueueFamilyIndex = target.family;
        acquire.buffer = target.pixels;
        acquire.size = VK_WHOLE_SIZE;
        vkCmdPipelineBarrier(target.commands[1], VK_PIPELINE_STAGE_ALL_COMMANDS_BIT, VK_PIPELINE_STAGE_ALL_COMMANDS_BIT,
                             0, 0, nullptr, 1, &acquire, 0, nullptr);
        check(vkEndCommandBuffer(target.commands[1]), "End presentation acquire");

        target.program.reset(new QOpenGLShaderProgram);
        if (!target.program->addShaderFromSourceCode(QOpenGLShader::Vertex,
            "attribute vec2 vertex; varying vec2 uv; void main(){uv=vec2((vertex.x+1.0)*0.5,(1.0-vertex.y)*0.5);gl_Position=vec4(vertex,0,1);}") ||
            !target.program->addShaderFromSourceCode(QOpenGLShader::Fragment,
            "uniform sampler2D video; varying vec2 uv; void main(){gl_FragColor=texture2D(video,uv);}") || !target.program->link())
            throw std::runtime_error("OpenGL presentation shader: " + target.program->log().toStdString());
        const float vertices[] = {-1,-1, 1,-1, -1,1, 1,1};
        GLint previousVertexArray = 0;
        gl->glGetIntegerv(GL_VERTEX_ARRAY_BINDING, &previousVertexArray);
        extra->glGenVertexArrays(1, &target.vertex_array);
        extra->glBindVertexArray(target.vertex_array);
        gl->glGenBuffers(1, &target.buffer);
        gl->glBindBuffer(GL_ARRAY_BUFFER, target.buffer);
        gl->glBufferData(GL_ARRAY_BUFFER, sizeof(vertices), vertices, GL_STATIC_DRAW);
        const int vertex = target.program->attributeLocation("vertex");
        target.program->enableAttributeArray(vertex);
        target.program->setAttributeBuffer(vertex, GL_FLOAT, 0, 2);
        gl->glBindBuffer(GL_ARRAY_BUFFER, 0);
        extra->glBindVertexArray(previousVertexArray);
    }
    bool draw(int x, int y, int width, int height) {
        if (qEnvironmentVariable("OPENSHOT_PRESENTATION_BACKEND") == "cpu")
            return unavailable("OPENSHOT_PRESENTATION_BACKEND=cpu (Vulkan processing remains enabled)");
        auto lastStage = std::chrono::steady_clock::now();
        const bool trace = qEnvironmentVariableIsSet("OPENSHOT_TRACE_PRESENTATION");
        auto stage = [&](const char* name) {
            if (!trace) return;
            auto now = std::chrono::steady_clock::now();
            qWarning() << "GPU presentation" << name << std::chrono::duration<double, std::milli>(now-lastStage).count();
            lastStage = now;
        };
        auto context = QOpenGLContext::currentContext();
        if (!context) return unavailable("No current Qt OpenGL context");
        if (logged_context != context) {
            auto text = [&](GLenum name) {
                const auto* value = context->functions()->glGetString(name);
                return value ? reinterpret_cast<const char*>(value) : "unknown";
            };
            Logger::Instance()->Log(std::string("GPU presentation Qt context: vendor=") + text(GL_VENDOR) +
                "; renderer=" + text(GL_RENDERER) + "; version=" + text(GL_VERSION) +
                "; OpenGL ES=" + (context->isOpenGLES() ? "yes" : "no") +
                "; external memory=" + (context->hasExtension(kMemoryExtension) ? "yes" : "no") +
                "; external semaphore=" + (context->hasExtension(kSemaphoreExtension) ? "yes" : "no"), Logger::LevelInfo);
            logged_context = context;
            identity_logged = false;
        }
        stage("begin draw");
        if (context->isOpenGLES())
            return unavailable("Vulkan presentation requires desktop OpenGL; Qt selected OpenGL ES/ANGLE (try QT_OPENGL=desktop)");
        if (!frame || !widget) return unavailable("No video frame or preview widget");
        if (fallback_delivered) return false;
        auto authoritative = frame->GetDeferredImage();
        if (!authoritative) return unavailable("Frame has no deferred GPU surface");
        auto surface = display_surface ? display_surface : std::dynamic_pointer_cast<const VulkanFrameImage>(authoritative);
        if (!surface) return unavailable("Frame cannot be presented as a Vulkan RGBA surface");
        auto inputContext = reinterpret_cast<AVHWFramesContext*>(surface->GetFrame()->hw_frames_ctx->data);
        if (inputContext->sw_format != AV_PIX_FMT_RGBA) {
            stage("begin RGBA conversion");
            surface = std::dynamic_pointer_cast<const VulkanFrameImage>(RenderVulkanImage(
                surface, nullptr, surface->GetWidth(), surface->GetHeight(), QTransform(), 1.0, Qt::black));
            if (!surface) return unavailable("Frame cannot be presented as a Vulkan RGBA surface");
        }
        display_surface = surface;
        stage("surface");
        const AVFrame* source = surface->GetFrame();
        auto fc = reinterpret_cast<AVHWFramesContext*>(source->hw_frames_ctx->data);
        auto vfc = static_cast<AVVulkanFramesContext*>(fc->hwctx);
        auto dc = fc->device_ctx;
        auto vk = static_cast<AVVulkanDeviceContext*>(dc->hwctx);
        auto image = reinterpret_cast<AVVkFrame*>(source->data[0]);
        // Multi-plane decode surfaces must first go through the RGBA compositor.
        if (fc->sw_format != AV_PIX_FMT_RGBA || vfc->format[0] != VK_FORMAT_R8G8B8A8_UNORM || image->img[1])
            return unavailable("Unsupported Vulkan image format for OpenGL sharing");
        if (!(vfc->usage & VK_IMAGE_USAGE_TRANSFER_SRC_BIT))
            return unavailable("Vulkan frame cannot be copied for OpenGL sharing");
        VkPhysicalDeviceIDProperties identity{VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_ID_PROPERTIES};
        VkPhysicalDeviceProperties2 properties{VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_PROPERTIES_2, &identity};
        vkGetPhysicalDeviceProperties2(vk->phys_dev, &properties);
        const bool logIdentity = !identity_logged;
        if (logIdentity) {
            Logger::Instance()->Log(std::string("GPU presentation Vulkan device: ") + properties.properties.deviceName +
                "; UUID=" + QByteArray(reinterpret_cast<const char*>(identity.deviceUUID), VK_UUID_SIZE).toHex().toStdString(),
                Logger::LevelInfo);
            identity_logged = true;
        }
        if (!context->hasExtension(kMemoryExtension) || !context->hasExtension(kSemaphoreExtension))
            return unavailable("OpenGL driver lacks external memory or semaphore sharing extensions");
        auto gl = context->functions();
        auto extra = context->extraFunctions();
        auto getUuid = glFunction<PFNGLGETUNSIGNEDBYTEI_VEXTPROC>(context, "glGetUnsignedBytei_vEXT");
        GLint deviceCount = 0;
        gl->glGetIntegerv(GL_NUM_DEVICE_UUIDS_EXT, &deviceCount);
        bool sameDevice = false;
        for (int i = 0; i < deviceCount; ++i) {
            GLubyte uuid[GL_UUID_SIZE_EXT];
            getUuid(GL_DEVICE_UUID_EXT, i, uuid);
            if (logIdentity)
                Logger::Instance()->Log("GPU presentation OpenGL device UUID=" +
                    QByteArray(reinterpret_cast<const char*>(uuid), GL_UUID_SIZE_EXT).toHex().toStdString(), Logger::LevelInfo);
            sameDevice = sameDevice || std::memcmp(uuid, identity.deviceUUID, VK_UUID_SIZE) == 0;
        }
        if (!sameDevice) {
            const auto* renderer = gl->glGetString(GL_RENDERER);
            return unavailable(std::string("Qt/OpenGL and Vulkan selected different GPUs (OpenGL: ") +
                (renderer ? reinterpret_cast<const char*>(renderer) : "unknown") +
                "; Vulkan: " + properties.properties.deviceName + ")");
        }
        if (vk->queue_family_comp_index < 0)
            return unavailable("Vulkan device lacks a compute queue");

        bool locked = false, copySubmitted = false;
        uint64_t copyValue = 0;
        try {
            if (target.context != context || target.device != vk->act_dev ||
                target.width != source->width || target.height != source->height) {
                releaseTarget();
                createTarget(context, fc, source->width, source->height);
                stage("create shared buffer");
            }
            // The previous frame's commands must retire before re-recording.
            waitPending();
            stage("previous frame");
            const auto submit = [&](const VkSubmitInfo& info, VkFence signalFence) {
                vk->lock_queue(dc, target.family, 0);
                const auto result = vkQueueSubmit(target.queue, 1, &info, signalFence);
                vk->unlock_queue(dc, target.family, 0);
                check(result, "Submit presentation transfer");
            };
            vfc->lock_frame(fc, image);
            locked = true;
            VkCommandBuffer copy = target.commands[0];
            VkCommandBufferBeginInfo begin{VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO};
            begin.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
            check(vkBeginCommandBuffer(copy, &begin), "Begin presentation copy");
            // FFmpeg frames are concurrent or owned by the compute family, so
            // the copy needs no ownership transfer of the source.
            VkImageMemoryBarrier toCopy{VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER};
            toCopy.srcAccessMask = image->access[0];
            toCopy.dstAccessMask = VK_ACCESS_TRANSFER_READ_BIT;
            toCopy.oldLayout = image->layout[0];
            toCopy.newLayout = VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL;
            toCopy.srcQueueFamilyIndex = toCopy.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
            toCopy.image = image->img[0];
            toCopy.subresourceRange = kColor;
            vkCmdPipelineBarrier(copy, VK_PIPELINE_STAGE_ALL_COMMANDS_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT,
                                 0, 0, nullptr, 0, nullptr, 1, &toCopy);
            VkBufferImageCopy region{};
            region.imageSubresource = {VK_IMAGE_ASPECT_COLOR_BIT, 0, 0, 1};
            region.imageExtent = {static_cast<uint32_t>(target.width), static_cast<uint32_t>(target.height), 1};
            vkCmdCopyImageToBuffer(copy, image->img[0], VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, target.pixels, 1, &region);
            VkBufferMemoryBarrier release{VK_STRUCTURE_TYPE_BUFFER_MEMORY_BARRIER};
            release.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
            release.srcQueueFamilyIndex = target.family;
            release.dstQueueFamilyIndex = VK_QUEUE_FAMILY_EXTERNAL;
            release.buffer = target.pixels;
            release.size = VK_WHOLE_SIZE;
            vkCmdPipelineBarrier(copy, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_ALL_COMMANDS_BIT,
                                 0, 0, nullptr, 1, &release, 0, nullptr);
            check(vkEndCommandBuffer(copy), "End presentation copy");
            // Advance the frame's timeline and release the copy to OpenGL.
            const uint64_t waitValue = image->sem_value[0];
            const uint64_t signalValues[2] = {image->sem_value[0] + 1, 0};
            const VkSemaphore copySignals[2] = {image->sem[0], target.handoff[0]};
            VkTimelineSemaphoreSubmitInfo timeline{VK_STRUCTURE_TYPE_TIMELINE_SEMAPHORE_SUBMIT_INFO};
            timeline.waitSemaphoreValueCount = 1;
            timeline.pWaitSemaphoreValues = &waitValue;
            timeline.signalSemaphoreValueCount = 2;
            timeline.pSignalSemaphoreValues = signalValues;
            const VkPipelineStageFlags waitStage = VK_PIPELINE_STAGE_ALL_COMMANDS_BIT;
            VkSubmitInfo copyInfo{VK_STRUCTURE_TYPE_SUBMIT_INFO, &timeline};
            copyInfo.waitSemaphoreCount = 1;
            copyInfo.pWaitSemaphores = &image->sem[0];
            copyInfo.pWaitDstStageMask = &waitStage;
            copyInfo.commandBufferCount = 1;
            copyInfo.pCommandBuffers = &copy;
            copyInfo.signalSemaphoreCount = 2;
            copyInfo.pSignalSemaphores = copySignals;
            submit(copyInfo, VK_NULL_HANDLE);
            copySubmitted = true;
            copyValue = ++image->sem_value[0];
            image->layout[0] = VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL;
            image->access[0] = VK_ACCESS_TRANSFER_READ_BIT;
            vfc->unlock_frame(fc, image);
            locked = false;
            stage("copy submit");

            target.wait_semaphore(target.gl_semaphores[0], 1, &target.gl_pixels, 0, nullptr, nullptr);
            GLint viewport[4], previousVertexArray = 0, previousUnpack = 0, unpackAlignment = 4, unpackRowLength = 0;
            gl->glGetIntegerv(GL_VIEWPORT, viewport);
            gl->glGetIntegerv(GL_VERTEX_ARRAY_BINDING, &previousVertexArray);
            gl->glGetIntegerv(GL_PIXEL_UNPACK_BUFFER_BINDING, &previousUnpack);
            gl->glGetIntegerv(GL_UNPACK_ALIGNMENT, &unpackAlignment);
            gl->glGetIntegerv(GL_UNPACK_ROW_LENGTH, &unpackRowLength);
            gl->glActiveTexture(GL_TEXTURE0);
            gl->glBindTexture(GL_TEXTURE_2D, target.texture);
            gl->glBindBuffer(GL_PIXEL_UNPACK_BUFFER, target.gl_pixels);
            gl->glPixelStorei(GL_UNPACK_ALIGNMENT, 4);
            gl->glPixelStorei(GL_UNPACK_ROW_LENGTH, 0);
            gl->glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, target.width, target.height, GL_RGBA, GL_UNSIGNED_BYTE, nullptr);
            gl->glBindBuffer(GL_PIXEL_UNPACK_BUFFER, previousUnpack);
            gl->glPixelStorei(GL_UNPACK_ALIGNMENT, unpackAlignment);
            gl->glPixelStorei(GL_UNPACK_ROW_LENGTH, unpackRowLength);
            gl->glViewport(x, y, width, height);
            gl->glDisable(GL_DEPTH_TEST);
            gl->glDisable(GL_SCISSOR_TEST);
            gl->glDisable(GL_STENCIL_TEST);
            gl->glDisable(GL_BLEND);
            gl->glColorMask(GL_TRUE, GL_TRUE, GL_TRUE, GL_TRUE);
            target.program->bind();
            target.program->setUniformValue("video", 0);
            extra->glBindVertexArray(target.vertex_array);
            gl->glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);
            if (trace) {
                const auto error = gl->glGetError();
                GLint framebuffer = 0, activeProgram = 0, boundTexture = 0;
                gl->glGetIntegerv(GL_FRAMEBUFFER_BINDING, &framebuffer);
                gl->glGetIntegerv(GL_CURRENT_PROGRAM, &activeProgram);
                gl->glGetIntegerv(GL_TEXTURE_BINDING_2D, &boundTexture);
                qWarning() << "GPU presentation draw state: error=" << error
                           << "framebuffer=" << framebuffer << "program=" << activeProgram
                           << "texture=" << boundTexture;
            }
            extra->glBindVertexArray(previousVertexArray);
            target.program->release();
            gl->glViewport(viewport[0], viewport[1], viewport[2], viewport[3]);
            // Signals once the unpack above has finished reading the buffer.
            target.signal_semaphore(target.gl_semaphores[1], 1, &target.gl_pixels, 0, nullptr, nullptr);
            gl->glFlush();
            stage("GL draw flush");

            VkSubmitInfo acquireInfo{VK_STRUCTURE_TYPE_SUBMIT_INFO};
            acquireInfo.waitSemaphoreCount = 1;
            acquireInfo.pWaitSemaphores = &target.handoff[1];
            acquireInfo.pWaitDstStageMask = &waitStage;
            acquireInfo.commandBufferCount = 1;
            acquireInfo.pCommandBuffers = &target.commands[1];
            submit(acquireInfo, target.fence);
            target.pending = true;
            stage("acquire submit");
            return true;
        } catch (...) {
            if (locked) vfc->unlock_frame(fc, image);
            // A half-finished handoff leaves the semaphores unusable.
            if (copySubmitted && !target.pending) {
                VkSemaphoreWaitInfo copyDone{VK_STRUCTURE_TYPE_SEMAPHORE_WAIT_INFO};
                copyDone.semaphoreCount = 1;
                copyDone.pSemaphores = &image->sem[0];
                copyDone.pValues = &copyValue;
                vkWaitSemaphores(target.device, &copyDone, UINT64_MAX);
            }
            releaseTarget();
            throw;
        }
    }
};
VulkanOpenGLPresenter::VulkanOpenGLPresenter(uintptr_t widget)
    : impl(new Impl(reinterpret_cast<QWidget*>(widget))) {}
VulkanOpenGLPresenter::~VulkanOpenGLPresenter() = default;
bool VulkanOpenGLPresenter::Draw(int x, int y, int width, int height) {
    try {
        if (impl->draw(x, y, width, height)) { impl->last_failure.clear(); ++impl->native_draws; return true; }
    } catch (const std::exception& error) {
        impl->last_failure = error.what();
        qWarning() << "Vulkan presentation fallback:" << error.what();
    }
    impl->fallback();
    return false;
}
int VulkanOpenGLPresenter::Width() const { return impl->frame ? impl->frame->GetWidth() : 0; }
int VulkanOpenGLPresenter::Height() const { return impl->frame ? impl->frame->GetHeight() : 0; }
void VulkanOpenGLPresenter::Clear() {
    impl->frame.reset();
    impl->display_surface.reset();
}
uint64_t VulkanOpenGLPresenter::NativeDraws() const { return impl->native_draws; }
uint64_t VulkanOpenGLPresenter::FallbackFrames() const { return impl->fallback_frames; }
std::string VulkanOpenGLPresenter::FallbackReason() const { return impl->last_failure; }
void VulkanOpenGLPresenter::Present(std::shared_ptr<Frame> frame) {
    if (QThread::currentThread() == impl->thread()) {
        impl->PresentFrame(frame);
    } else {
        QMetaObject::invokeMethod(impl.get(), [receiver=impl.get(), frame]() {
            receiver->PresentFrame(frame);
        }, Qt::QueuedConnection);
    }
}
}
