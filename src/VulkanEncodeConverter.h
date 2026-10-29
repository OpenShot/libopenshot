// Copyright (c) 2026 OpenShot Studios, LLC
// SPDX-License-Identifier: LGPL-3.0-or-later
#ifndef OPENSHOT_VULKAN_ENCODE_CONVERTER_H
#define OPENSHOT_VULKAN_ENCODE_CONVERTER_H
#include "FrameImage.h"
#include <memory>
namespace openshot {
/// Convert an RGBA Vulkan surface to encoder-ready limited-range NV12 on the GPU.
/// shareable uses linear DMA-BUF images that VA-API can import without a copy.
std::shared_ptr<const FrameImage> ConvertVulkanToNV12(std::shared_ptr<const FrameImage> source, bool shareable = false);
}
#endif
