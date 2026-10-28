// Copyright (c) 2026 OpenShot Studios, LLC
// SPDX-License-Identifier: LGPL-3.0-or-later
#include "VulkanOpenGLPresenter.h"
#include "VideoRenderer.h"
#include "../VulkanFrameImage.h"
#include "../VulkanCompositor.h"
#include <QOpenGLContext>
#include <QOpenGLFunctions>
#include <QOpenGLExtraFunctions>
#include <QOpenGLShaderProgram>
#include <QPointer>
#include <QWidget>
#include <QVariant>
#include <QDebug>
#include <QThread>
#include <GL/glext.h>
#include <unistd.h>
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
}
struct VulkanOpenGLPresenter::Impl : QObject, FramePresenter {
    QPointer<QWidget> widget;
    std::shared_ptr<Frame> frame;
    std::shared_ptr<const VulkanFrameImage> display_surface;
    bool fallback_delivered = false;
    uint64_t native_draws = 0;
    uint64_t fallback_frames = 0;
    explicit Impl(QWidget* value) : widget(value) {
        value->setProperty("openshot_gpu_presenter", QVariant::fromValue(static_cast<QObject*>(this)));
    }
    ~Impl() override {
        if (widget) widget->setProperty("openshot_gpu_presenter", QVariant());
    }
    void PresentFrame(const std::shared_ptr<Frame>& value) override {
        frame = value;
        display_surface.reset();
        fallback_delivered = false;
        if (frame && !frame->GetDeferredImage()) fallback();
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
    bool draw(int x, int y, int width, int height) {
        auto lastStage = std::chrono::steady_clock::now();
        const bool trace = qEnvironmentVariableIsSet("OPENSHOT_TRACE_PRESENTATION");
        auto stage = [&](const char* name) {
            if (!trace) return;
            auto now = std::chrono::steady_clock::now();
            qWarning() << "GPU presentation" << name << std::chrono::duration<double, std::milli>(now-lastStage).count();
            lastStage = now;
        };
        auto context = QOpenGLContext::currentContext();
        if (!context || !frame || !widget || fallback_delivered) return false;
        auto authoritative = frame->GetDeferredImage();
        if (!authoritative) return false;
        auto surface = display_surface ? display_surface : std::dynamic_pointer_cast<const VulkanFrameImage>(authoritative);
        if (!surface) return false;
        auto inputContext = reinterpret_cast<AVHWFramesContext*>(surface->GetFrame()->hw_frames_ctx->data);
        if (inputContext->sw_format != AV_PIX_FMT_RGBA) {
            surface = std::dynamic_pointer_cast<const VulkanFrameImage>(RenderVulkanImage(
                surface, nullptr, surface->GetWidth(), surface->GetHeight(), QTransform(), 1.0, Qt::black));
            if (!surface) return false;
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
            return false;
        if (!context->hasExtension("GL_EXT_memory_object_fd") || !context->hasExtension("GL_EXT_semaphore_fd"))
            return false;
        auto gl = context->functions();
        auto extra = context->extraFunctions();
        auto getUuid = glFunction<PFNGLGETUNSIGNEDBYTEI_VEXTPROC>(context, "glGetUnsignedBytei_vEXT");
        VkPhysicalDeviceIDProperties identity{VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_ID_PROPERTIES};
        VkPhysicalDeviceProperties2 properties{VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_PROPERTIES_2, &identity};
        vkGetPhysicalDeviceProperties2(vk->phys_dev, &properties);
        GLint deviceCount = 0;
        gl->glGetIntegerv(GL_NUM_DEVICE_UUIDS_EXT, &deviceCount);
        bool sameDevice = false;
        for (int i = 0; i < deviceCount; ++i) {
            GLubyte uuid[GL_UUID_SIZE_EXT];
            getUuid(GL_DEVICE_UUID_EXT, i, uuid);
            sameDevice = sameDevice || std::memcmp(uuid, identity.deviceUUID, VK_UUID_SIZE) == 0;
        }
        if (!sameDevice) return false;
        auto createMemory = glFunction<PFNGLCREATEMEMORYOBJECTSEXTPROC>(context, "glCreateMemoryObjectsEXT");
        auto deleteMemory = glFunction<PFNGLDELETEMEMORYOBJECTSEXTPROC>(context, "glDeleteMemoryObjectsEXT");
        auto memoryParameter = glFunction<PFNGLMEMORYOBJECTPARAMETERIVEXTPROC>(context, "glMemoryObjectParameterivEXT");
        auto importMemory = glFunction<PFNGLIMPORTMEMORYFDEXTPROC>(context, "glImportMemoryFdEXT");
        auto textureMemory = glFunction<PFNGLTEXSTORAGEMEM2DEXTPROC>(context, "glTexStorageMem2DEXT");
        auto createSemaphore = glFunction<PFNGLGENSEMAPHORESEXTPROC>(context, "glGenSemaphoresEXT");
        auto deleteSemaphore = glFunction<PFNGLDELETESEMAPHORESEXTPROC>(context, "glDeleteSemaphoresEXT");
        auto importSemaphore = glFunction<PFNGLIMPORTSEMAPHOREFDEXTPROC>(context, "glImportSemaphoreFdEXT");
        auto waitSemaphore = glFunction<PFNGLWAITSEMAPHOREEXTPROC>(context, "glWaitSemaphoreEXT");
        auto signalSemaphore = glFunction<PFNGLSIGNALSEMAPHOREEXTPROC>(context, "glSignalSemaphoreEXT");
        auto getMemoryFd = reinterpret_cast<PFN_vkGetMemoryFdKHR>(vkGetDeviceProcAddr(vk->act_dev, "vkGetMemoryFdKHR"));
        auto getSemaphoreFd = reinterpret_cast<PFN_vkGetSemaphoreFdKHR>(vkGetDeviceProcAddr(vk->act_dev, "vkGetSemaphoreFdKHR"));
        if (!getMemoryFd || !getSemaphoreFd || vk->queue_family_comp_index < 0) return false;
        // Construct all fallible GL resources before transferring image ownership.
        QOpenGLShaderProgram program;
        if (!program.addShaderFromSourceCode(QOpenGLShader::Vertex,
            "attribute vec2 vertex; varying vec2 uv; void main(){uv=vec2((vertex.x+1.0)*0.5,(1.0-vertex.y)*0.5);gl_Position=vec4(vertex,0,1);}") ||
            !program.addShaderFromSourceCode(QOpenGLShader::Fragment,
            "uniform sampler2D video; varying vec2 uv; void main(){gl_FragColor=texture2D(video,uv);}") || !program.link()) return false;
        stage("shader");
        VkDevice device = vk->act_dev;
        GLuint memory = 0, texture = 0, semaphores[2] = {0, 0}, buffer = 0, vertexArray = 0;
        GLint previousVertexArray = 0;
        gl->glGetIntegerv(GL_VERTEX_ARRAY_BINDING, &previousVertexArray);
        VkSemaphore handoff[2] = {VK_NULL_HANDLE, VK_NULL_HANDLE};
        VkCommandPool pool = VK_NULL_HANDLE;
        VkFence fence = VK_NULL_HANDLE;
        int memoryFd = -1, semaphoreFd = -1;
        auto cleanup = [&]() {
            if (vertexArray) {
                extra->glBindVertexArray(previousVertexArray);
                extra->glDeleteVertexArrays(1, &vertexArray);
            }
            if (buffer) gl->glDeleteBuffers(1, &buffer);
            if (texture) gl->glDeleteTextures(1, &texture);
            stage("delete texture");
            if (memory) deleteMemory(1, &memory);
            stage("delete memory");
            deleteSemaphore(2, semaphores);
            stage("delete GL semaphore");
            if (fence) vkDestroyFence(device, fence, nullptr);
            if (pool) vkDestroyCommandPool(device, pool, nullptr);
            for (auto semaphore : handoff)
                if (semaphore) vkDestroySemaphore(device, semaphore, nullptr);
            stage("delete Vulkan resources");
            if (memoryFd >= 0) close(memoryFd);
            if (semaphoreFd >= 0) close(semaphoreFd);
        };
        bool locked = false;
        try {
            VkMemoryGetFdInfoKHR memoryInfo{VK_STRUCTURE_TYPE_MEMORY_GET_FD_INFO_KHR};
            memoryInfo.memory = image->mem[0];
            memoryInfo.handleType = VK_EXTERNAL_MEMORY_HANDLE_TYPE_OPAQUE_FD_BIT;
            check(getMemoryFd(device, &memoryInfo, &memoryFd), "Export Vulkan image memory");
            createMemory(1, &memory);
            VkMemoryDedicatedRequirements dedicated{VK_STRUCTURE_TYPE_MEMORY_DEDICATED_REQUIREMENTS};
            VkMemoryRequirements2 requirements{VK_STRUCTURE_TYPE_MEMORY_REQUIREMENTS_2, &dedicated};
            VkImageMemoryRequirementsInfo2 requirementInfo{VK_STRUCTURE_TYPE_IMAGE_MEMORY_REQUIREMENTS_INFO_2};
            requirementInfo.image = image->img[0];
            vkGetImageMemoryRequirements2(device, &requirementInfo, &requirements);
            GLint useDedicated = dedicated.prefersDedicatedAllocation || dedicated.requiresDedicatedAllocation;
            memoryParameter(memory, GL_DEDICATED_MEMORY_OBJECT_EXT, &useDedicated);
            importMemory(memory, image->size[0], GL_HANDLE_TYPE_OPAQUE_FD_EXT, memoryFd);
            memoryFd = -1; // GL consumes imported descriptors.
            stage("import memory");
            gl->glGenTextures(1, &texture);
            gl->glBindTexture(GL_TEXTURE_2D, texture);
            gl->glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
            gl->glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
            gl->glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
            gl->glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
            textureMemory(GL_TEXTURE_2D, 1, GL_RGBA8, source->width, source->height, memory, image->offset[0]);
            stage("texture storage");
            VkExportSemaphoreCreateInfo exportInfo{VK_STRUCTURE_TYPE_EXPORT_SEMAPHORE_CREATE_INFO};
            exportInfo.handleTypes = VK_EXTERNAL_SEMAPHORE_HANDLE_TYPE_OPAQUE_FD_BIT;
            VkSemaphoreCreateInfo semaphoreInfo{VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO, &exportInfo};
            // Separate directions: reusing one binary semaphore lets Vulkan's
            // acquire consume its own release signal before GL can wait on it.
            createSemaphore(2, semaphores);
            for (int direction = 0; direction < 2; ++direction) {
                check(vkCreateSemaphore(device, &semaphoreInfo, nullptr, &handoff[direction]), "Create presentation semaphore");
                VkSemaphoreGetFdInfoKHR fdInfo{VK_STRUCTURE_TYPE_SEMAPHORE_GET_FD_INFO_KHR};
                fdInfo.semaphore = handoff[direction];
                fdInfo.handleType = VK_EXTERNAL_SEMAPHORE_HANDLE_TYPE_OPAQUE_FD_BIT;
                check(getSemaphoreFd(device, &fdInfo, &semaphoreFd), "Export presentation semaphore");
                importSemaphore(semaphores[direction], GL_HANDLE_TYPE_OPAQUE_FD_EXT, semaphoreFd);
                semaphoreFd = -1;
            }
            if (gl->glGetError() != GL_NO_ERROR) throw std::runtime_error("OpenGL external memory import failed");
            const uint32_t family = vk->queue_family_comp_index;
            VkQueue queue;
            vkGetDeviceQueue(device, family, 0, &queue);
            VkCommandPoolCreateInfo poolInfo{VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO};
            poolInfo.queueFamilyIndex = family;
            check(vkCreateCommandPool(device, &poolInfo, nullptr, &pool), "Create presentation command pool");
            VkCommandBuffer commands[2];
            VkCommandBufferAllocateInfo allocation{VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO};
            allocation.commandPool = pool;
            allocation.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
            allocation.commandBufferCount = 2;
            check(vkAllocateCommandBuffers(device, &allocation, commands), "Allocate presentation commands");
            VkFenceCreateInfo fenceInfo{VK_STRUCTURE_TYPE_FENCE_CREATE_INFO};
            check(vkCreateFence(device, &fenceInfo, nullptr, &fence), "Create presentation fence");
            const float vertices[] = {-1,-1, 1,-1, -1,1, 1,1};
            extra->glGenVertexArrays(1, &vertexArray);
            extra->glBindVertexArray(vertexArray);
            gl->glGenBuffers(1, &buffer);
            gl->glBindBuffer(GL_ARRAY_BUFFER, buffer);
            gl->glBufferData(GL_ARRAY_BUFFER, sizeof(vertices), vertices, GL_STATIC_DRAW);
            program.bind();
            program.setUniformValue("video", 0);
            const int vertex = program.attributeLocation("vertex");
            program.enableAttributeArray(vertex);
            program.setAttributeBuffer(vertex, GL_FLOAT, 0, 2);
            vfc->lock_frame(fc, image);
            locked = true;
            stage("frame lock");
            const auto oldLayout = image->layout[0];
            const auto oldOwner = image->queue_family[0];
            for (int i = 0; i < 2; ++i) {
                VkCommandBufferBeginInfo begin{VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO};
                check(vkBeginCommandBuffer(commands[i], &begin), "Begin presentation commands");
                VkImageMemoryBarrier barrier{VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER};
                barrier.srcAccessMask = i ? 0 : image->access[0];
                barrier.dstAccessMask = i ? VK_ACCESS_MEMORY_READ_BIT | VK_ACCESS_MEMORY_WRITE_BIT : 0;
                barrier.oldLayout = i ? VK_IMAGE_LAYOUT_GENERAL : oldLayout;
                barrier.newLayout = VK_IMAGE_LAYOUT_GENERAL;
                barrier.srcQueueFamilyIndex = i ? VK_QUEUE_FAMILY_EXTERNAL : oldOwner;
                barrier.dstQueueFamilyIndex = i ? family : VK_QUEUE_FAMILY_EXTERNAL;
                barrier.image = image->img[0];
                barrier.subresourceRange = {VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1};
                vkCmdPipelineBarrier(commands[i], VK_PIPELINE_STAGE_ALL_COMMANDS_BIT, VK_PIPELINE_STAGE_ALL_COMMANDS_BIT, 0, 0, nullptr, 0, nullptr, 1, &barrier);
                check(vkEndCommandBuffer(commands[i]), "End presentation commands");
            }
            auto submit = [&](int index, VkSemaphore wait, uint64_t waitValue, VkSemaphore signal, uint64_t signalValue) {
                VkTimelineSemaphoreSubmitInfo timeline{VK_STRUCTURE_TYPE_TIMELINE_SEMAPHORE_SUBMIT_INFO};
                timeline.waitSemaphoreValueCount = timeline.signalSemaphoreValueCount = 1;
                timeline.pWaitSemaphoreValues = &waitValue;
                timeline.pSignalSemaphoreValues = &signalValue;
                VkPipelineStageFlags stage = VK_PIPELINE_STAGE_ALL_COMMANDS_BIT;
                VkSubmitInfo info{VK_STRUCTURE_TYPE_SUBMIT_INFO, &timeline};
                info.waitSemaphoreCount = info.signalSemaphoreCount = info.commandBufferCount = 1;
                info.pWaitSemaphores = &wait;
                info.pWaitDstStageMask = &stage;
                info.pSignalSemaphores = &signal;
                info.pCommandBuffers = &commands[index];
                vk->lock_queue(dc, family, 0);
                auto result = vkQueueSubmit(queue, 1, &info, index ? fence : VK_NULL_HANDLE);
                vk->unlock_queue(dc, family, 0);
                check(result, "Submit presentation transfer");
            };
            submit(0, image->sem[0], image->sem_value[0], handoff[0], 0);
            stage("release submit");
            const GLenum layout = GL_LAYOUT_GENERAL_EXT;
            waitSemaphore(semaphores[0], 0, nullptr, 1, &texture, &layout);
            GLint viewport[4];
            gl->glGetIntegerv(GL_VIEWPORT, viewport);
            gl->glViewport(x, y, width, height);
            gl->glActiveTexture(GL_TEXTURE0);
            gl->glBindTexture(GL_TEXTURE_2D, texture);
            gl->glDisable(GL_DEPTH_TEST);
            gl->glDisable(GL_SCISSOR_TEST);
            gl->glDisable(GL_STENCIL_TEST);
            gl->glDisable(GL_BLEND);
            gl->glColorMask(GL_TRUE, GL_TRUE, GL_TRUE, GL_TRUE);
            gl->glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);
            signalSemaphore(semaphores[1], 0, nullptr, 1, &texture, &layout);
            gl->glFlush();
            stage("GL draw flush");
            submit(1, handoff[1], 0, image->sem[0], image->sem_value[0] + 1);
            image->sem_value[0]++;
            image->layout[0] = VK_IMAGE_LAYOUT_GENERAL;
            image->access[0] = VK_ACCESS_MEMORY_READ_BIT;
            image->queue_family[0] = family;
            vfc->unlock_frame(fc, image);
            locked = false;
            // Synchronous prototype: no resource reuse before GL and Vulkan finish.
            check(vkWaitForFences(device, 1, &fence, VK_TRUE, UINT64_MAX), "Wait for presentation transfer");
            stage("acquire fence");
            gl->glViewport(viewport[0], viewport[1], viewport[2], viewport[3]);
            program.disableAttributeArray(vertex);
            program.release();
            gl->glBindBuffer(GL_ARRAY_BUFFER, 0);
            // Retire GL's imported-object command references as well as the
            // Vulkan queue work before destroying the cross-API resources.
            gl->glFinish();
            stage("GL finish");
            cleanup();
            stage("cleanup");
            return true;
        } catch (...) {
            if (locked) vfc->unlock_frame(fc, image);
            cleanup();
            throw;
        }
    }
};
VulkanOpenGLPresenter::VulkanOpenGLPresenter(uintptr_t widget)
    : impl(new Impl(reinterpret_cast<QWidget*>(widget))) {}
VulkanOpenGLPresenter::~VulkanOpenGLPresenter() = default;
bool VulkanOpenGLPresenter::Draw(int x, int y, int width, int height) {
    try {
        if (impl->draw(x, y, width, height)) { ++impl->native_draws; return true; }
    } catch (const std::exception& error) {
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
