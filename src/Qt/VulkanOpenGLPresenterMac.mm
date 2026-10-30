// Copyright (c) 2026 OpenShot Studios, LLC
// SPDX-License-Identifier: LGPL-3.0-or-later
#define GL_SILENCE_DEPRECATION
#include "VulkanOpenGLPresenter.h"
#include "VideoRenderer.h"
#include "../VulkanFrameImage.h"
#include "../VulkanCompositor.h"
#include "../MacVulkanInterop.h"
#include <QOpenGLContext>
#include <QOpenGLFunctions>
#include <QOpenGLExtraFunctions>
#include <QOpenGLShaderProgram>
#include <QPointer>
#include <QWidget>
#include <QVariant>
#include <QDebug>
#include <QThread>
#import <OpenGL/OpenGL.h>
#import <OpenGL/CGLIOSurface.h>
#import <OpenGL/CGLRenderers.h>
#import <CoreVideo/CoreVideo.h>
#include <stdexcept>
extern "C" {
#include <libavutil/hwcontext.h>
}
namespace openshot {
namespace {
bool SameDevice(CGLContextObj context, uint64_t registry) {
    if (!context || !registry) return false;
    GLint current = 0;
    if (CGLGetParameter(context, kCGLCPCurrentRendererID, &current) != kCGLNoError) return false;
    CGLRendererInfoObj info = nullptr;
    GLint count = 0;
    if (CGLQueryRendererInfo(0xffffffffu, &info, &count) != kCGLNoError) return false;
    bool matched = false;
    for (GLint i = 0; i < count; ++i) {
        GLint renderer = 0, low = 0, high = 0;
        CGLDescribeRenderer(info, i, kCGLRPRendererID, &renderer);
        if ((renderer & kCGLRendererIDMatchingMask) != (current & kCGLRendererIDMatchingMask)) continue;
        if (CGLDescribeRenderer(info, i, kCGLRPRegistryIDLow, &low) == kCGLNoError &&
            CGLDescribeRenderer(info, i, kCGLRPRegistryIDHigh, &high) == kCGLNoError)
            matched |= ((uint64_t(uint32_t(high)) << 32) | uint32_t(low)) == registry;
    }
    CGLDestroyRendererInfo(info);
    return matched;
}
}
struct VulkanOpenGLPresenter::Impl : QObject, FramePresenter {
    QPointer<QWidget> widget;
    std::shared_ptr<Frame> frame;
    std::shared_ptr<const VulkanFrameImage> display_surface;
    bool fallback_delivered = false;
    uint64_t native_draws = 0, fallback_frames = 0;
    explicit Impl(QWidget* value) : widget(value) {
        value->setProperty("openshot_gpu_presenter", QVariant::fromValue(static_cast<QObject*>(this)));
    }
    ~Impl() override { if (widget) widget->setProperty("openshot_gpu_presenter", QVariant()); }
    void fallback() {
        if (!fallback_delivered && frame && widget) {
            fallback_delivered = true; ++fallback_frames; display_surface.reset();
            const QImage image = *frame->GetImage();
            QMetaObject::invokeMethod(widget, "present", Qt::QueuedConnection, Q_ARG(QImage, image));
        }
    }
    void PresentFrame(const std::shared_ptr<Frame>& value) override {
        frame = value; display_surface.reset(); fallback_delivered = false;
        if (frame && !frame->GetDeferredImage()) fallback();
        if (widget) {
            if (frame && frame->GetDeferredImage() && widget->metaObject()->indexOfMethod("gpu_presented(int)") >= 0)
                QMetaObject::invokeMethod(widget, "gpu_presented", Qt::DirectConnection,
                    Q_ARG(int, static_cast<int>(frame->number)));
            widget->update();
        }
    }
    bool draw(int x, int y, int width, int height) {
        auto* context = QOpenGLContext::currentContext();
        auto cgl = CGLGetCurrentContext();
        if (!context || !cgl || !frame || !widget || fallback_delivered) return false;
        auto surface = display_surface ? display_surface : std::dynamic_pointer_cast<const VulkanFrameImage>(frame->GetDeferredImage());
        if (!surface) return false;
        auto* fc = reinterpret_cast<const AVHWFramesContext*>(surface->GetFrame()->hw_frames_ctx->data);
        if (fc->sw_format != AV_PIX_FMT_RGBA || !IsMacVulkanFrame(surface->GetFrame())) {
            surface = std::dynamic_pointer_cast<const VulkanFrameImage>(RenderVulkanImage(
                surface, nullptr, surface->GetWidth(), surface->GetHeight(), QTransform(), 1.0, Qt::black));
            if (!surface) return false;
        }
        const AVFrame* source = surface->GetFrame();
        auto pixel = static_cast<CVPixelBufferRef>(MacVulkanPixelBuffer(source));
        if (!pixel || !SameDevice(cgl, MacVulkanDeviceRegistryID(source))) return false;
        display_surface = surface;
        WaitMacVulkanFrame(source);
        auto io = CVPixelBufferGetIOSurface(pixel);
        if (!io) return false;
        auto* gl = context->functions();
        auto* extra = context->extraFunctions();
        const bool core = context->format().profile() == QSurfaceFormat::CoreProfile;
        QOpenGLShaderProgram program;
        const char* vertex = core ?
            "#version 150\nin vec2 vertex;out vec2 uv;void main(){uv=vec2((vertex.x+1.0)*0.5,(1.0-vertex.y)*0.5);gl_Position=vec4(vertex,0,1);}" :
            "#version 120\nattribute vec2 vertex;varying vec2 uv;void main(){uv=vec2((vertex.x+1.0)*0.5,(1.0-vertex.y)*0.5);gl_Position=vec4(vertex,0,1);}";
        // CGL's supported 32-bit IOSurface view is BGRA. Our compositor stores
        // RGBA bytes, so undo the view's red/blue interpretation in the sampler.
        const char* fragment = core ?
            "#version 150\nuniform sampler2DRect video;uniform vec2 size;in vec2 uv;out vec4 color;void main(){color=texture(video,uv*size).bgra;}" :
            "#version 120\n#extension GL_ARB_texture_rectangle : enable\nuniform sampler2DRect video;uniform vec2 size;varying vec2 uv;void main(){gl_FragColor=texture2DRect(video,uv*size).bgra;}";
        if (!program.addShaderFromSourceCode(QOpenGLShader::Vertex, vertex) ||
            !program.addShaderFromSourceCode(QOpenGLShader::Fragment, fragment) || !program.link())
            throw std::runtime_error(program.log().toStdString());
        GLuint texture = 0, buffer = 0, vao = 0;
        GLint oldViewport[4], oldVao = 0;
        gl->glGetIntegerv(GL_VIEWPORT, oldViewport);
        if (core) gl->glGetIntegerv(GL_VERTEX_ARRAY_BINDING, &oldVao);
        auto cleanup = [&]() {
            if (buffer) gl->glDeleteBuffers(1, &buffer);
            if (vao) { extra->glBindVertexArray(oldVao); extra->glDeleteVertexArrays(1, &vao); }
            if (texture) gl->glDeleteTextures(1, &texture);
            gl->glViewport(oldViewport[0], oldViewport[1], oldViewport[2], oldViewport[3]);
        };
        try {
            gl->glActiveTexture(GL_TEXTURE0);
            gl->glGenTextures(1, &texture);
            gl->glBindTexture(GL_TEXTURE_RECTANGLE, texture);
            const auto imported = CGLTexImageIOSurface2D(cgl, GL_TEXTURE_RECTANGLE, GL_RGBA8,
                source->width, source->height, GL_BGRA, GL_UNSIGNED_INT_8_8_8_8_REV, io, 0);
            if (imported != kCGLNoError) throw std::runtime_error("CGL shared IOSurface import failed");
            gl->glTexParameteri(GL_TEXTURE_RECTANGLE, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
            gl->glTexParameteri(GL_TEXTURE_RECTANGLE, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
            gl->glTexParameteri(GL_TEXTURE_RECTANGLE, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
            gl->glTexParameteri(GL_TEXTURE_RECTANGLE, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
            if (core) { extra->glGenVertexArrays(1, &vao); extra->glBindVertexArray(vao); }
            const float vertices[] = {-1,-1, 1,-1, -1,1, 1,1};
            gl->glGenBuffers(1, &buffer); gl->glBindBuffer(GL_ARRAY_BUFFER, buffer);
            gl->glBufferData(GL_ARRAY_BUFFER, sizeof(vertices), vertices, GL_STATIC_DRAW);
            if (!program.bind()) throw std::runtime_error("Bind Mac presentation shader");
            program.setUniformValue("video", 0);
            program.setUniformValue("size", QVector2D(source->width, source->height));
            const int attribute = program.attributeLocation("vertex");
            program.enableAttributeArray(attribute); program.setAttributeBuffer(attribute, GL_FLOAT, 0, 2);
            gl->glViewport(x, y, width, height);
            gl->glDisable(GL_DEPTH_TEST); gl->glDisable(GL_SCISSOR_TEST);
            gl->glDisable(GL_STENCIL_TEST); gl->glDisable(GL_BLEND);
            gl->glColorMask(GL_TRUE, GL_TRUE, GL_TRUE, GL_TRUE);
            gl->glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);
            // The next renderer may reuse the retained surface after PresentFrame.
            // Complete GL sampling before releasing our reference to that IOSurface.
            gl->glFinish();
            const auto error = gl->glGetError();
            program.disableAttributeArray(attribute); program.release();
            gl->glBindBuffer(GL_ARRAY_BUFFER, 0);
            if (error != GL_NO_ERROR) throw std::runtime_error("Mac shared-texture presentation failed");
            cleanup();
            return true;
        } catch (...) { cleanup(); throw; }
    }
};
VulkanOpenGLPresenter::VulkanOpenGLPresenter(uintptr_t widget) : impl(new Impl(reinterpret_cast<QWidget*>(widget))) {}
VulkanOpenGLPresenter::~VulkanOpenGLPresenter() = default;
bool VulkanOpenGLPresenter::Draw(int x, int y, int width, int height) {
    try { if (impl->draw(x,y,width,height)) { ++impl->native_draws; return true; } }
    catch (const std::exception& error) { qWarning() << "Mac Vulkan presentation fallback:" << error.what(); }
    impl->fallback(); return false;
}
int VulkanOpenGLPresenter::Width() const { return impl->frame ? impl->frame->GetWidth() : 0; }
int VulkanOpenGLPresenter::Height() const { return impl->frame ? impl->frame->GetHeight() : 0; }
void VulkanOpenGLPresenter::Clear() { impl->frame.reset(); impl->display_surface.reset(); }
uint64_t VulkanOpenGLPresenter::NativeDraws() const { return impl->native_draws; }
uint64_t VulkanOpenGLPresenter::FallbackFrames() const { return impl->fallback_frames; }
void VulkanOpenGLPresenter::Present(std::shared_ptr<Frame> frame) {
    if (QThread::currentThread() == impl->thread()) impl->PresentFrame(frame);
    else QMetaObject::invokeMethod(impl.get(), [receiver=impl.get(), frame]() { receiver->PresentFrame(frame); }, Qt::QueuedConnection);
}
}
