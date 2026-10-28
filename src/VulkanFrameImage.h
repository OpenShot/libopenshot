// Copyright (c) 2026 OpenShot Studios, LLC
// SPDX-License-Identifier: LGPL-3.0-or-later
#ifndef OPENSHOT_VULKAN_FRAME_IMAGE_H
#define OPENSHOT_VULKAN_FRAME_IMAGE_H
#include "FrameImage.h"
#include <memory>
struct AVFrame;
namespace openshot {
/// A retained FFmpeg Vulkan image. CPU pixels are created only by Materialize().
class VulkanFrameImage final : public FrameImage {
public:
    explicit VulkanFrameImage(const AVFrame* frame, bool premultiplied = false);
    ~VulkanFrameImage() override;
    VulkanFrameImage(const VulkanFrameImage&) = delete;
    VulkanFrameImage& operator=(const VulkanFrameImage&) = delete;
    int GetWidth() const override;
    int GetHeight() const override;
    int64_t GetBytes() const override;
    QImage Materialize() const override;
    /// Borrowed immutable frame; retain a reference before asynchronous use.
    const AVFrame* GetFrame() const;
    bool IsPremultiplied() const;
private:
    struct Impl;
    std::unique_ptr<Impl> impl;
};
/// Reuses the scaling pipeline across frames. Outputs own independent storage.
class VulkanFrameProcessor {
public:
    VulkanFrameProcessor();
    ~VulkanFrameProcessor();
    VulkanFrameProcessor(const VulkanFrameProcessor&) = delete;
    VulkanFrameProcessor& operator=(const VulkanFrameProcessor&) = delete;
    std::shared_ptr<const FrameImage> Process(const AVFrame* frame, int width, int height, bool high_quality = false);
    void Reset();
private:
    struct Impl;
    std::unique_ptr<Impl> impl;
};
}
#endif
