// Copyright (c) 2026 OpenShot Studios, LLC
// SPDX-License-Identifier: LGPL-3.0-or-later

// Temporary switches for debugging GPU test builds; search for "GpuDebug"
// to find (and later remove) every use.
//   OPENSHOT_GPU_DEBUG=1                  extra logging only, no behavior change
//   OPENSHOT_PRESENTATION_TEST=pattern    preview shows solid magenta instead of video
//   OPENSHOT_PRESENTATION_SYNC=wait       CPU waits instead of Vulkan/OpenGL semaphores
//   OPENSHOT_PRESENTATION_DEDICATED=0|1   force the preview buffer's memory allocation style
#ifndef OPENSHOT_GPU_DEBUG_H
#define OPENSHOT_GPU_DEBUG_H

#include <QString>

namespace openshot {

// OPENSHOT_TRACE_PRESENTATION is the older name for the same logging.
inline bool GpuDebug() {
    static const bool enabled = qEnvironmentVariableIsSet("OPENSHOT_GPU_DEBUG") ||
                                qEnvironmentVariableIsSet("OPENSHOT_TRACE_PRESENTATION");
    return enabled;
}

inline QString GpuDebugSetting(const char* name) {
    return qEnvironmentVariable(name).trimmed().toLower();
}

}

#endif
