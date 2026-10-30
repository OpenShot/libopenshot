// Copyright (c) 2026 OpenShot Studios, LLC
// SPDX-License-Identifier: LGPL-3.0-or-later
#include "openshot_catch.h"
#include "Frame.h"
#include "VulkanFrameImage.h"
#include "GpuDevices.h"
#include "Qt/VideoRenderer.h"
#include "Qt/VulkanOpenGLPresenter.h"
#include <QApplication>
#include <QOpenGLContext>
#include <QOpenGLWidget>
#include <QPainter>
#include <QThread>
extern "C" {
#include <libavutil/hwcontext.h>
}
namespace {
class Preview : public QOpenGLWidget {
public:
    openshot::VulkanOpenGLPresenter presenter{reinterpret_cast<uintptr_t>(this)};
    bool nativeDraw = false;
    void paintGL() override {
        QPainter painter(this);
        painter.fillRect(rect(), Qt::blue);
        painter.beginNativePainting();
        nativeDraw = presenter.Draw(0, 0, width() * devicePixelRatioF(), height() * devicePixelRatioF());
        painter.endNativePainting();
        painter.fillRect(QRect(0, 0, 20, 20), Qt::green);
    }
};
struct Frames {
    AVBufferRef* device = nullptr;
    AVBufferRef* pool = nullptr;
    AVFrame* cpu = av_frame_alloc();
    AVFrame* gpu = av_frame_alloc();
    ~Frames() {
        av_frame_free(&cpu);
        av_frame_free(&gpu);
        av_buffer_unref(&pool);
        av_buffer_unref(&device);
    }
};
}
TEST_CASE("Vulkan presentation keeps image on GPU and preserves Qt overlays", "[vulkan][presentation]") {
    // CTest reports "Skipping Vulkan test:" as skipped; see tests/CMakeLists.txt.
    if (!openshot::VulkanRenderingEnabled()) {
        WARN("Skipping Vulkan test: OPENSHOT_RENDER_BACKEND=cpu");
        return;
    }
    const QByteArray platform = qgetenv("QT_QPA_PLATFORM");
#ifdef __linux__
    if ((platform.isEmpty() || platform.startsWith("xcb")) && !qEnvironmentVariableIsSet("DISPLAY") &&
        !qEnvironmentVariableIsSet("WAYLAND_DISPLAY")) {
        WARN("Skipping Vulkan test: presentation needs a display with OpenGL");
        return;
    }
#endif
    if (platform == "offscreen" || platform == "minimal") {
        WARN("Skipping Vulkan test: presentation needs a native OpenGL display");
        return;
    }
    Frames data;
    if (openshot::CreateSharedVulkanDevice(&data.device) < 0) {
        WARN("Skipping Vulkan test: no Vulkan device is available");
        return;
    }
    int argc = 1;
    char name[] = "vulkan-presentation";
    char* argv[] = {name, nullptr};
    QApplication app(argc, argv);
    data.pool = av_hwframe_ctx_alloc(data.device);
    REQUIRE(data.pool);
    auto pool = reinterpret_cast<AVHWFramesContext*>(data.pool->data);
    pool->format = AV_PIX_FMT_VULKAN;
    pool->sw_format = AV_PIX_FMT_RGBA;
    pool->width = pool->height = 64;
    REQUIRE(av_hwframe_ctx_init(data.pool) >= 0);
    REQUIRE(av_hwframe_get_buffer(data.pool, data.gpu, 0) >= 0);
    data.cpu->format = AV_PIX_FMT_RGBA;
    data.cpu->width = data.cpu->height = 64;
    REQUIRE(av_frame_get_buffer(data.cpu, 32) >= 0);
    for (int y = 0; y < 64; ++y) {
        for (int x = 0; x < 64; ++x) {
            auto p = data.cpu->data[0] + y * data.cpu->linesize[0] + x * 4;
            p[0] = y < 32 ? 255 : 0; p[1] = 0; p[2] = y < 32 ? 0 : 255; p[3] = 255;
        }
    }
    REQUIRE(av_hwframe_transfer_data(data.gpu, data.cpu, 0) >= 0);
    auto frame = std::make_shared<openshot::Frame>();
    frame->AddDeferredImage(std::make_shared<openshot::VulkanFrameImage>(data.gpu, true));
    Preview widget;
    widget.resize(128, 128);
    VideoRenderer renderer;
    renderer.OverrideWidget(reinterpret_cast<uintptr_t>(&widget));
    widget.show();
    QCoreApplication::processEvents();
    auto* context = widget.context();
    if (!context
#ifdef _WIN32
        || !context->hasExtension("GL_EXT_memory_object_win32") || !context->hasExtension("GL_EXT_semaphore_win32")
#elif !defined(__APPLE__)
        || !context->hasExtension("GL_EXT_memory_object_fd") || !context->hasExtension("GL_EXT_semaphore_fd")
#endif
    ) {
        WARN("Skipping Vulkan test: OpenGL lacks Vulkan memory/semaphore interop");
        return;
    }
    const auto readbacks = openshot::VulkanFrameReadbackCount();
    renderer.paint(frame);
    for (int i = 0; i < 20; ++i) {
        QCoreApplication::processEvents();
        QThread::msleep(5);
    }
    QImage image;
    // Exercise both semaphore directions repeatedly; a single frame can hide
    // a race where Vulkan consumes its own release signal before GL waits.
    for (int i = 0; i < 64; ++i) {
        image = widget.grabFramebuffer();
        REQUIRE(widget.nativeDraw);
        REQUIRE(image.pixelColor(image.width()/2, image.height()/4) == QColor(Qt::red));
        REQUIRE(image.pixelColor(image.width()/2, 3*image.height()/4) == QColor(Qt::blue));
    }
    REQUIRE(widget.nativeDraw);
    CHECK(widget.presenter.NativeDraws() >= 64);
    CHECK(widget.presenter.FallbackFrames() == 0);
    CHECK(openshot::VulkanFrameReadbackCount() == readbacks);
    REQUIRE(frame->GetDeferredImage());
    REQUIRE_FALSE(image.isNull());
    if (qEnvironmentVariableIsSet("OPENSHOT_PRESENTATION_CAPTURE"))
        image.save(qEnvironmentVariable("OPENSHOT_PRESENTATION_CAPTURE"));
    INFO("center=" << image.pixelColor(image.width()/2, image.height()/2).name().toStdString()
        << " overlay=" << image.pixelColor(5, 5).name().toStdString());
    CHECK(image.pixelColor(image.width()/2, image.height()/4) == QColor(Qt::red));
    CHECK(image.pixelColor(image.width()/2, 3*image.height()/4) == QColor(Qt::blue));
    CHECK(image.pixelColor(5, 5) == QColor(Qt::green));
}
