// Copyright (c) 2026 OpenShot Studios, LLC
// SPDX-License-Identifier: LGPL-3.0-or-later
#ifndef OPENSHOT_VULKAN_OPENGL_PRESENTER_H
#define OPENSHOT_VULKAN_OPENGL_PRESENTER_H
#include <cstdint>
#include <memory>
#include <string>
namespace openshot {
class Frame;
/// GUI-thread adapter for a QOpenGLWidget. Draw requires its current GL context.
class VulkanOpenGLPresenter {
public:
    explicit VulkanOpenGLPresenter(uintptr_t widget);
    ~VulkanOpenGLPresenter();
    bool Draw(int x, int y, int width, int height);
    int Width() const;
    int Height() const;
    void Clear();
    uint64_t NativeDraws() const;
    uint64_t FallbackFrames() const;
    /// Most recent native presentation failure; empty after a successful draw.
    std::string FallbackReason() const;
    void Present(std::shared_ptr<Frame> frame);
private:
    struct Impl;
    std::unique_ptr<Impl> impl;
};
}
#endif
