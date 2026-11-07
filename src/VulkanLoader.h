// Copyright (c) 2026 OpenShot Studios, LLC
// SPDX-License-Identifier: LGPL-3.0-or-later
//
// Loads the Vulkan functions OpenShot uses at runtime, so libopenshot starts on
// systems without a Vulkan loader or driver (they render on the CPU instead).
// Build with VK_NO_PROTOTYPES; each pointer below has the same name as the
// Vulkan function, so calling code is unchanged.
#pragma once
#include <vulkan/vulkan.h>

#define OPENSHOT_VULKAN_FUNCTIONS(X) \
    X(vkAllocateCommandBuffers) X(vkAllocateDescriptorSets) X(vkAllocateMemory) \
    X(vkBeginCommandBuffer) X(vkBindBufferMemory) X(vkCmdBindDescriptorSets) \
    X(vkCmdBindPipeline) X(vkCmdCopyBuffer) X(vkCmdCopyImageToBuffer) X(vkCmdDispatch) \
    X(vkCmdFillBuffer) X(vkCmdPipelineBarrier) X(vkCmdPushConstants) X(vkCreateBuffer) \
    X(vkCreateCommandPool) X(vkCreateComputePipelines) X(vkCreateDescriptorPool) \
    X(vkCreateDescriptorSetLayout) X(vkCreateFence) X(vkCreateImage) X(vkCreateImageView) \
    X(vkCreateInstance) X(vkCreatePipelineLayout) X(vkCreateSampler) X(vkCreateSemaphore) \
    X(vkCreateShaderModule) X(vkDestroyBuffer) X(vkDestroyCommandPool) \
    X(vkDestroyDescriptorPool) X(vkDestroyDescriptorSetLayout) X(vkDestroyFence) \
    X(vkDestroyImage) X(vkDestroyImageView) X(vkDestroyInstance) X(vkDestroyPipeline) \
    X(vkDestroyPipelineLayout) X(vkDestroySampler) X(vkDestroySemaphore) \
    X(vkDestroyShaderModule) X(vkDeviceWaitIdle) X(vkEndCommandBuffer) \
    X(vkEnumerateDeviceExtensionProperties) X(vkEnumerateInstanceVersion) \
    X(vkEnumeratePhysicalDevices) X(vkFlushMappedMemoryRanges) X(vkFreeMemory) \
    X(vkGetBufferMemoryRequirements) X(vkGetBufferMemoryRequirements2) \
    X(vkGetDeviceProcAddr) X(vkGetDeviceQueue) X(vkGetPhysicalDeviceMemoryProperties) \
    X(vkGetPhysicalDeviceProperties) X(vkGetPhysicalDeviceProperties2) \
    X(vkGetPhysicalDeviceQueueFamilyProperties) X(vkInvalidateMappedMemoryRanges) \
    X(vkMapMemory) X(vkQueueSubmit) X(vkResetFences) X(vkUnmapMemory) \
    X(vkUpdateDescriptorSets) X(vkWaitForFences) X(vkWaitSemaphores)

// Hidden, so other libraries in the process never bind to these pointers
#if defined(_WIN32)
#define OPENSHOT_VULKAN_HIDDEN
#else
#define OPENSHOT_VULKAN_HIDDEN __attribute__((visibility("hidden")))
#endif
#define OPENSHOT_VULKAN_DECLARE(name) extern OPENSHOT_VULKAN_HIDDEN PFN_##name name;
OPENSHOT_VULKAN_FUNCTIONS(OPENSHOT_VULKAN_DECLARE)
#undef OPENSHOT_VULKAN_DECLARE

namespace openshot {
// True when the Vulkan library and every function above were found. Cheap
// after the first call; nothing may call Vulkan when it returns false.
bool LoadVulkan();
}
