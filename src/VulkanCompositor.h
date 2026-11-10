// Copyright (c) 2026 OpenShot Studios, LLC
// SPDX-License-Identifier: LGPL-3.0-or-later
#ifndef OPENSHOT_VULKAN_COMPOSITOR_H
#define OPENSHOT_VULKAN_COMPOSITOR_H
#include "FrameImage.h"
#include <memory>
#include <array>
#include <cstdint>
#include <vector>
#include <QColor>
#include <QTransform>
#include <QRectF>
namespace openshot {
namespace color { struct SourceColor; }
/// Scale a decoded YUV surface to RGBA using its color description: YUV
/// matrix and range, and for HDR or BT.2020 sources conversion to SDR BT.709
/// matching color::SdrConverter on the CPU.
std::shared_ptr<const FrameImage> DecodeVulkanImage(std::shared_ptr<const FrameImage> source,
    int width, int height, const color::SourceColor& source_color, bool high_quality = false);
/// Affine transform and source-over compositing into an independently owned GPU frame.
/// Inputs must be Vulkan surfaces from the same device; null background uses clear.
/// high_quality selects antialiased bicubic reader resampling; default matches Qt bilinear transforms.
std::shared_ptr<const FrameImage> RenderVulkanImage(
    std::shared_ptr<const FrameImage> source, std::shared_ptr<const FrameImage> background,
    int width, int height, const QTransform& source_to_output, double opacity,
    const QColor& clear = Qt::transparent, bool high_quality = false);
/// Apply common and per-channel saturation, preserving premultiplied alpha.
std::shared_ptr<const FrameImage> SaturateVulkanImage(
    std::shared_ptr<const FrameImage> source, const std::array<float, 4>& saturation);
/// Copy a source rectangle into a transparent output, with optional resized canvas.
std::shared_ptr<const FrameImage> CropVulkanImage(std::shared_ptr<const FrameImage> source,
    const QRectF& destination, const QRectF& copy, const QRect& canvas);
/// Per-frame grading parameters; curves retain the CPU's 256-entry lookup semantics.
struct VulkanGradeParameters {
    std::array<float,9> controls; // temperature, tint, exposure gain, contrast, shadows, highlights, saturation, vibrance, mix
    std::array<float,12> wheels; // RGB bias for global, shadows, midtones, highlights
    std::array<std::array<float,256>,4> curves; // master, red, green, blue
};
std::shared_ptr<const FrameImage> GradeVulkanImage(std::shared_ptr<const FrameImage> source,
    const VulkanGradeParameters& parameters);
/// Apply a 1D or 3D .cube LUT as ColorMap does. The table holds the domain
/// minimum and maximum (3 floats each), then the RGB entries. Pass the same
/// table object each frame so the GPU copy is reused.
std::shared_ptr<const FrameImage> LookupVulkanImage(std::shared_ptr<const FrameImage> source,
    std::shared_ptr<const std::vector<float>> table, int size, bool cube, const std::array<float,3>& intensity);
/// Blend an effect result over its input by a grayscale mask, as EffectBase does.
/// drive selects the squared "drive amount" response, which keeps the input alpha.
std::shared_ptr<const FrameImage> BlendMaskVulkanImage(std::shared_ptr<const FrameImage> original,
    std::shared_ptr<const FrameImage> effected, std::shared_ptr<const FrameImage> mask, bool invert, bool drive = false);
/// Count FrameScope video data for a region on the GPU. Returns luma, red,
/// green and blue histograms (256 each), the same four waveforms
/// (columns x bins each), then the vectorscope (size x size).
std::vector<uint32_t> ScopeVulkanImage(std::shared_ptr<const FrameImage> source, const QRect& region,
    int waveform_columns, int waveform_bins, int vectorscope_size);
/// Apply a reader mask using the existing brightness/contrast and inversion semantics.
std::shared_ptr<const FrameImage> MaskVulkanImage(std::shared_ptr<const FrameImage> source,
    std::shared_ptr<const FrameImage> mask, float contrast, int brightness, bool invert, bool replace);
/// Separable box blur with clamped region edges and CPU-compatible compositing.
std::shared_ptr<const FrameImage> BlurVulkanImage(std::shared_ptr<const FrameImage> source,
    const QRect& area, int horizontal, int vertical, int iterations);
/// Diagnostic count of successful compute pipeline creations.
uint64_t VulkanPipelineCreationCount();
/// Return pooled GPU frame memory once its frames are released, i.e. after
/// clearing caches. Later renders create new pools as needed.
void ReleaseVulkanMemory();
}
#endif
