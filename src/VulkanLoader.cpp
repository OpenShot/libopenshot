// Copyright (c) 2026 OpenShot Studios, LLC
// SPDX-License-Identifier: LGPL-3.0-or-later
#include "VulkanLoader.h"
#include "Logger.h"
#include <string>
#if defined(_WIN32)
#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>
#else
#include <dlfcn.h>
#endif

#define OPENSHOT_VULKAN_DEFINE(name) PFN_##name name = nullptr;
OPENSHOT_VULKAN_FUNCTIONS(OPENSHOT_VULKAN_DEFINE)
#undef OPENSHOT_VULKAN_DEFINE

namespace openshot {
namespace {
bool Load() {
    // Never unloaded: drivers keep threads and state for the process lifetime.
#if defined(_WIN32)
    HMODULE library = LoadLibraryExA("vulkan-1.dll", nullptr, LOAD_LIBRARY_SEARCH_DEFAULT_DIRS);
    const auto symbol = [library](const char* name) { return reinterpret_cast<void*>(GetProcAddress(library, name)); };
#elif defined(__APPLE__)
    // MoltenVK ships inside the app and is linked directly
    void* library = RTLD_DEFAULT;
    const auto symbol = [](const char* name) { return dlsym(RTLD_DEFAULT, name); };
#else
    void* library = dlopen("libvulkan.so.1", RTLD_NOW | RTLD_LOCAL);
    const auto symbol = [library](const char* name) { return dlsym(library, name); };
#endif
    if (!library) {
        Logger::Instance()->Log("Vulkan library not found; rendering on the CPU", Logger::LevelWarning);
        return false;
    }
    bool complete = true;
#define OPENSHOT_VULKAN_LOAD(name) \
    name = reinterpret_cast<PFN_##name>(symbol(#name)); \
    if (!name) { \
        Logger::Instance()->Log("Vulkan library lacks " #name "; rendering on the CPU", Logger::LevelWarning); \
        complete = false; \
    }
    OPENSHOT_VULKAN_FUNCTIONS(OPENSHOT_VULKAN_LOAD)
#undef OPENSHOT_VULKAN_LOAD
    return complete;
}
}  // namespace

bool LoadVulkan() {
    static const bool loaded = Load();
    return loaded;
}
}  // namespace openshot
