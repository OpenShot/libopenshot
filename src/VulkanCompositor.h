// Copyright (c) 2026 OpenShot Studios, LLC
// SPDX-License-Identifier: LGPL-3.0-or-later
#ifndef OPENSHOT_VULKAN_COMPOSITOR_H
#define OPENSHOT_VULKAN_COMPOSITOR_H
#include "FrameImage.h"
#include <memory>
#include <QColor>
#include <QTransform>
namespace openshot {
/// Affine transform and source-over compositing into an independently owned GPU frame.
/// Inputs must be Vulkan surfaces from the same device; null background uses clear.
/// high_quality selects antialiased bicubic reader resampling; default matches Qt bilinear transforms.
std::shared_ptr<const FrameImage> RenderVulkanImage(
    std::shared_ptr<const FrameImage> source, std::shared_ptr<const FrameImage> background,
    int width, int height, const QTransform& source_to_output, double opacity,
    const QColor& clear = Qt::transparent, bool high_quality = false);
/// Return pooled GPU frame memory once its frames are released, i.e. after
/// clearing caches. Later renders create new pools as needed.
void ReleaseVulkanMemory();
}
#endif
