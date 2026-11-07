// Copyright (c) 2026 OpenShot Studios, LLC
// SPDX-License-Identifier: LGPL-3.0-or-later
// A lost device is permanent for the process, so this test has its own binary.
#include "openshot_catch.h"
#include "FFmpegReader.h"
#include "GpuDevices.h"
#include "VulkanFrameImage.h"
extern "C" {
#include <libavutil/hwcontext.h>
#include <libavutil/hwcontext_vulkan.h>
}

TEST_CASE("A lost GPU device moves open readers and new work to the CPU", "[vulkan][recovery]") {
    if (!openshot::VulkanRenderingEnabled()) {
        WARN("Skipping Vulkan test: CPU mode, or no Vulkan library");
        return;
    }
    AVBufferRef* device = nullptr;
    if (openshot::CreateSharedVulkanDevice(&device) < 0) {
        WARN("Skipping Vulkan test: no Vulkan device is available");
        return;
    }
    av_buffer_unref(&device);
    CHECK(openshot::VulkanResultName(VK_ERROR_DEVICE_LOST) == "VK_ERROR_DEVICE_LOST");
    openshot::FFmpegReader reader(TEST_MEDIA_PATH "sintel_trailer-720p.mp4");
    reader.Open();
    const bool gpu_decoded = reader.GetFrame(1)->GetDeferredImage() != nullptr;

    // Other errors (such as low GPU memory) are recoverable.
    openshot::ReportVulkanResult(VK_ERROR_OUT_OF_DEVICE_MEMORY);
    CHECK_FALSE(openshot::VulkanDeviceLost());
    CHECK(openshot::VulkanRenderingEnabled());

    openshot::ReportVulkanResult(VK_ERROR_DEVICE_LOST);
    CHECK(openshot::VulkanDeviceLost());
    CHECK_FALSE(openshot::VulkanRenderingEnabled());
    // The open reader reopens with CPU decoding instead of using the lost device.
    auto frame = reader.GetFrame(48);
    REQUIRE(frame);
    CHECK_FALSE(frame->GetDeferredImage());
    CHECK(frame->GetImage()->width() == 1280);
    if (!gpu_decoded) WARN("GPU decode was not active before the loss on this system");
    reader.Close();
}
