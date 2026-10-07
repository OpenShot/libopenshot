// Copyright (c) 2008-2026 OpenShot Studios, LLC
// SPDX-License-Identifier: LGPL-3.0-or-later

#ifndef OPENSHOT_CAMERA_CAPTURE_V4L2_H
#define OPENSHOT_CAMERA_CAPTURE_V4L2_H

// Private, Linux-only enumeration helper. Injecting ioctl allows deterministic
// tests without opening a camera or changing its capture configuration.
#if defined(__linux__)
#include "CameraCaptureReader.h"
#include <linux/videodev2.h>
#include <cerrno>
#include <climits>
#include <functional>
#include <stdexcept>

namespace openshot { namespace detail {
using CameraIoctl = std::function<int(unsigned long, void*)>;

inline bool CameraQuery(const CameraIoctl& query, unsigned long request, void* value)
{
    int result;
    do { result = query(request, value); } while (result < 0 && errno == EINTR);
    if (result >= 0) return true;
    if (errno == EINVAL || errno == ENOTTY) return false;
    throw std::runtime_error("Unable to enumerate V4L2 camera modes (errno " + std::to_string(errno) + ").");
}

inline std::string CameraInputFormat(unsigned int format)
{
    switch (format) {
    case V4L2_PIX_FMT_MJPEG: return "mjpeg";
    case V4L2_PIX_FMT_JPEG: return "mjpeg";
    case V4L2_PIX_FMT_H264: return "h264";
    case V4L2_PIX_FMT_YUYV: return "yuyv422";
    case V4L2_PIX_FMT_UYVY: return "uyvy422";
    case V4L2_PIX_FMT_NV12: return "nv12";
    case V4L2_PIX_FMT_YUV420: return "yuv420p";
    case V4L2_PIX_FMT_RGB24: return "rgb24";
    case V4L2_PIX_FMT_BGR24: return "bgr24";
    case V4L2_PIX_FMT_GREY: return "gray";
    default: return ""; // Never pass an unrecognized FOURCC to FFmpeg.
    }
}

inline std::vector<CameraCaptureMode> EnumerateCameraModes(const CameraIoctl& query)
{
    std::vector<CameraCaptureMode> modes;
    auto add = [&](unsigned int width, unsigned int height, const std::string& format, v4l2_fract interval) {
        if (!width || !height || width > INT_MAX || height > INT_MAX ||
            !interval.numerator || !interval.denominator ||
            interval.numerator > INT_MAX || interval.denominator > INT_MAX) return;
        CameraCaptureMode mode;
        mode.width = width;
        mode.height = height;
        mode.input_format = format;
        mode.fps = Fraction(interval.denominator, interval.numerator);
        mode.fps.Reduce();
        for (const auto& existing : modes)
            if (existing.width == mode.width && existing.height == mode.height &&
                existing.input_format == mode.input_format && existing.fps.num == mode.fps.num &&
                existing.fps.den == mode.fps.den) return;
        modes.push_back(mode);
    };
    for (unsigned int f = 0; ; ++f) {
        v4l2_fmtdesc format = {};
        format.index = f;
        format.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
        if (!CameraQuery(query, VIDIOC_ENUM_FMT, &format)) break;
        const auto input_format = CameraInputFormat(format.pixelformat);
        if (input_format.empty()) continue;
        for (unsigned int s = 0; ; ++s) {
            v4l2_frmsizeenum size = {};
            size.index = s;
            size.pixel_format = format.pixelformat;
            if (!CameraQuery(query, VIDIOC_ENUM_FRAMESIZES, &size)) break;
            std::vector<std::pair<unsigned int, unsigned int>> sizes;
            if (size.type == V4L2_FRMSIZE_TYPE_DISCRETE)
                sizes.emplace_back(size.discrete.width, size.discrete.height);
            else if (size.type == V4L2_FRMSIZE_TYPE_STEPWISE || size.type == V4L2_FRMSIZE_TYPE_CONTINUOUS) {
                sizes.emplace_back(size.stepwise.min_width, size.stepwise.min_height);
                sizes.emplace_back(size.stepwise.max_width, size.stepwise.max_height);
            }
            for (const auto& dimensions : sizes) {
                for (unsigned int i = 0; ; ++i) {
                    v4l2_frmivalenum interval = {};
                    interval.index = i;
                    interval.pixel_format = format.pixelformat;
                    interval.width = dimensions.first;
                    interval.height = dimensions.second;
                    if (!CameraQuery(query, VIDIOC_ENUM_FRAMEINTERVALS, &interval)) break;
                    if (interval.type == V4L2_FRMIVAL_TYPE_DISCRETE)
                        add(interval.width, interval.height, input_format, interval.discrete);
                    else if (interval.type == V4L2_FRMIVAL_TYPE_STEPWISE || interval.type == V4L2_FRMIVAL_TYPE_CONTINUOUS) {
                        add(interval.width, interval.height, input_format, interval.stepwise.min);
                        add(interval.width, interval.height, input_format, interval.stepwise.max);
                    }
                    if (interval.type != V4L2_FRMIVAL_TYPE_DISCRETE) break;
                }
            }
            if (size.type != V4L2_FRMSIZE_TYPE_DISCRETE) break;
        }
    }
    return modes;
}
}}
#endif
#endif
