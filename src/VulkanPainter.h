// Copyright (c) 2026 OpenShot Studios, LLC
// SPDX-License-Identifier: LGPL-3.0-or-later
#ifndef OPENSHOT_VULKAN_PAINTER_H
#define OPENSHOT_VULKAN_PAINTER_H
#include "FrameImage.h"
#include <QPaintDevice>
#include <memory>
namespace openshot {
/// QPainter vector-command target. Only geometry and brush definitions cross to
/// the device; rasterization, gradients and compositing execute on the GPU.
class VulkanPaintDevice final : public QPaintDevice {
    struct Engine;
    std::unique_ptr<Engine> engine;
    int width,height;
protected:
    int metric(PaintDeviceMetric metric) const override;
public:
    VulkanPaintDevice(int width,int height);
    ~VulkanPaintDevice() override;
    QPaintEngine* paintEngine() const override;
    std::shared_ptr<const FrameImage> Render(std::shared_ptr<const FrameImage> source);
};
}
#endif
