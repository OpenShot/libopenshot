// Copyright (c) 2026 OpenShot Studios, LLC
// SPDX-License-Identifier: LGPL-3.0-or-later
#pragma once
extern "C" {
#include <libavutil/hwcontext_vulkan.h>
}

namespace openshot {
// Use only queues enabled on the FFmpeg device, in its preference order.
inline int VulkanComputeQueueFamily(const AVVulkanDeviceContext* context) {
#if LIBAVUTIL_VERSION_MAJOR >= 60
    for (int i = 0; i < context->nb_qf; ++i) {
        const auto& family = context->qf[i];
        if (family.idx >= 0 && family.num > 0 && (family.flags & VK_QUEUE_COMPUTE_BIT))
            return family.idx;
    }
    return -1;
#else
    return context->queue_family_comp_index;
#endif
}
}
