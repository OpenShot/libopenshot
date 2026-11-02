// SPDX-FileCopyrightText: 2026 OpenShot Studios, LLC
// SPDX-License-Identifier: LGPL-3.0-or-later
#pragma once

#include "FFmpegUtilities.h"
#include <libswscale/swscale.h>

namespace openshot {

inline int SourceSwsColorspace(AVColorSpace colorspace) {
    switch (colorspace) {
        case AVCOL_SPC_BT709:
            return SWS_CS_ITU709;
        case AVCOL_SPC_FCC:
            return SWS_CS_FCC;
        case AVCOL_SPC_BT470BG:
        case AVCOL_SPC_SMPTE170M:
            return SWS_CS_SMPTE170M;
        case AVCOL_SPC_SMPTE240M:
            return SWS_CS_SMPTE240M;
        case AVCOL_SPC_BT2020_NCL:
            return SWS_CS_BT2020;
        case AVCOL_SPC_BT2020_CL:
            return -1;
        default:
            return SWS_CS_DEFAULT;
    }
}

} // namespace openshot
