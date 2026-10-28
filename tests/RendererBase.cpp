// Copyright (c) 2026 OpenShot Studios, LLC
// SPDX-License-Identifier: LGPL-3.0-or-later
#include "openshot_catch.h"
#include "RendererBase.h"
#ifdef OPENSHOT_ENABLE_GPU_FRAME
#include "Qt/VideoRenderer.h"
#include <QApplication>
#include <QWidget>
#endif

namespace {
class ImageRenderer : public openshot::RendererBase {
public:
    std::shared_ptr<QImage> received;
    void OverrideWidget(uintptr_t) override {}
protected:
    void render(std::shared_ptr<QImage> image) override { received = image; }
};
#ifdef OPENSHOT_ENABLE_GPU_FRAME
class RetainedRenderer : public ImageRenderer {
public:
    std::shared_ptr<openshot::Frame> frame;
protected:
    void renderFrame(const std::shared_ptr<openshot::Frame>& value) override { frame = value; }
};
class DeferredImage : public openshot::FrameImage {
public:
    mutable int reads = 0;
    int GetWidth() const override { return 2; }
    int GetHeight() const override { return 2; }
    int64_t GetBytes() const override { return 16; }
    QImage Materialize() const override {
        ++reads;
        QImage result(2, 2, QImage::Format_RGBA8888_Premultiplied);
        result.fill(Qt::red);
        return result;
    }
};
class NativePresenter : public QWidget, public FramePresenter {
public:
    std::shared_ptr<openshot::Frame> received;
    void PresentFrame(const std::shared_ptr<openshot::Frame>& frame) override { received = frame; }
};
#endif
}

TEST_CASE("Image renderer preserves CPU presentation", "[renderer]") {
    ImageRenderer renderer;
    renderer.paint(nullptr);
    CHECK_FALSE(renderer.received);
    auto frame = std::make_shared<openshot::Frame>();
    frame->AddColor(2, 2, "#ff0000");
    renderer.paint(frame);
    REQUIRE(renderer.received);
    CHECK(renderer.received->pixelColor(0, 0) == QColor(Qt::red));
}

#ifdef OPENSHOT_ENABLE_GPU_FRAME
TEST_CASE("Frame renderer retains surfaces until CPU fallback is requested", "[renderer]") {
    auto surface = std::make_shared<DeferredImage>();
    auto frame = std::make_shared<openshot::Frame>();
    frame->AddDeferredImage(surface);
    RetainedRenderer retained;
    retained.paint(frame);
    CHECK(retained.frame == frame);
    CHECK(surface->reads == 0);
    CHECK_FALSE(retained.received);
    ImageRenderer fallback;
    fallback.paint(frame);
    CHECK(surface->reads == 1);
    REQUIRE(fallback.received);
    CHECK(fallback.received->pixelColor(0, 0) == QColor(Qt::red));
}

TEST_CASE("Qt presenter queues owned frames without materializing pixels", "[renderer]") {
    if (qEnvironmentVariableIsEmpty("DISPLAY") && qEnvironmentVariableIsEmpty("QT_QPA_PLATFORM"))
        qputenv("QT_QPA_PLATFORM", "offscreen");
    int argc = 1;
    char name[] = "renderer-test";
    char* argv[] = {name, nullptr};
    QApplication app(argc, argv);
    NativePresenter widget;
    VideoRenderer renderer;
    renderer.OverrideWidget(reinterpret_cast<uintptr_t>(&widget));
    auto surface = std::make_shared<DeferredImage>();
    auto frame = std::make_shared<openshot::Frame>();
    frame->AddDeferredImage(surface);
    std::weak_ptr<openshot::Frame> lifetime = frame;
    renderer.paint(frame);
    frame.reset();
    CHECK_FALSE(lifetime.expired());
    CHECK_FALSE(widget.received);
    QCoreApplication::processEvents();
    REQUIRE(widget.received);
    CHECK(surface->reads == 0);
    CHECK(widget.received->GetDeferredImage() == surface);
}
#endif
