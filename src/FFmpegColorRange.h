// SPDX-License-Identifier: LGPL-3.0-or-later
#pragma once

#include "FFmpegUtilities.h"

// Normalize deprecated JPEG-range YUVJ formats before creating swscale contexts.
// swscale expects non-YUVJ formats plus explicit color-range metadata.
inline AVPixelFormat NormalizeDeprecatedPixFmt(AVPixelFormat pix_fmt, bool& is_full_range) {
	switch (pix_fmt) {
		case AV_PIX_FMT_YUVJ420P:
			is_full_range = true;
			return AV_PIX_FMT_YUV420P;
		case AV_PIX_FMT_YUVJ422P:
			is_full_range = true;
			return AV_PIX_FMT_YUV422P;
		case AV_PIX_FMT_YUVJ444P:
			is_full_range = true;
			return AV_PIX_FMT_YUV444P;
		case AV_PIX_FMT_YUVJ440P:
			is_full_range = true;
			return AV_PIX_FMT_YUV440P;
#ifdef AV_PIX_FMT_YUVJ411P
		case AV_PIX_FMT_YUVJ411P:
			is_full_range = true;
			return AV_PIX_FMT_YUV411P;
#endif
		default:
			return pix_fmt;
	}
}
