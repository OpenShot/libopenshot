#!/usr/bin/env python3
# Copyright (c) 2026 OpenShot Studios, LLC
# SPDX-License-Identifier: LGPL-3.0-or-later
"""Generate the color chart fixtures used by tests/ColorManagement.cpp.

Each chart is a 4x4 grid of flat patches (80x48 px, 320x192 frame, 3 frames)
with known colors, (near-)losslessly encoded:

  examples/color-chart-sdr-bt709.mp4  8-bit H.264, BT.709 tagged
  examples/color-chart-hlg.mp4        10-bit HEVC, BT.2020 + HLG (iPhone style)
  examples/color-chart-pq.mp4         10-bit HEVC, BT.2020 + PQ (HDR10)

Patches are SDR code values (PATCHES in tests/ColorManagement.cpp). HDR charts
place them per ITU-R BT.2408: BT.1886 display light with 100% = 203 nits. The
last two patches are 600 and 1000 nit highlights.

Usage: generate-color-charts.py [ffmpeg] [output_dir]  (needs numpy, libx264, libx265)
"""
import subprocess
import sys

import numpy as np

FFMPEG = sys.argv[1] if len(sys.argv) > 1 else "ffmpeg"
OUT = sys.argv[2] if len(sys.argv) > 2 else "examples"
W, H, PW, PH = 320, 192, 80, 48
PATCHES = [(255, 255, 255), (118, 118, 118), (224, 172, 140), (141, 85, 62), (98, 155, 210),
           (87, 122, 59), (220, 30, 30), (40, 200, 40), (30, 40, 220), (240, 220, 40),
           (20, 20, 20), 600.0, 1000.0]
WHITE = 203.0
M709_TO_2020 = np.array([[0.6274, 0.3293, 0.0433], [0.0691, 0.9195, 0.0114], [0.0164, 0.0880, 0.8956]])
K709, K2020 = (0.2126, 0.0722), (0.2627, 0.0593)


def pq(nits):
    m1, m2, c1, c2, c3 = 2610 / 16384, 2523 / 4096 * 128, 3424 / 4096, 2413 / 4096 * 32, 2392 / 4096 * 32
    y = np.clip(nits / 10000.0, 0, 1) ** m1
    return ((c1 + c2 * y) / (1 + c3 * y)) ** m2


def hlg(nits):  # display light (BT.2020) -> HLG signal, 1000 nit display, gamma 1.2
    a, b, c = 0.17883277, 0.28466892, 0.55991073
    yd = nits @ np.array([K2020[0], 1 - sum(K2020), K2020[1]])
    ys = (np.maximum(yd, 1e-9) / 1000.0) ** (1 / 1.2)
    e = np.clip(nits / 1000.0 / np.maximum(ys ** 0.2, 1e-9)[..., None], 0, 1)
    return np.where(e <= 1 / 12, np.sqrt(3 * e), a * np.log(np.maximum(12 * e - b, 1e-12)) + c)


def chart_nits():
    image = np.zeros((H, W, 3))
    for i, patch in enumerate(PATCHES):
        r, c = divmod(i, 4)
        value = np.full(3, patch) if isinstance(patch, float) else (np.array(patch) / 255.0) ** 2.4 * WHITE
        image[r * PH:(r + 1) * PH, c * PW:(c + 1) * PW] = value
    return image


def write(name, rgb, k, bits, tags, encoder):
    kr, kb = k
    y = rgb @ np.array([kr, 1 - kr - kb, kb])
    cb, cr = (rgb[..., 2] - y) / (2 * (1 - kb)), (rgb[..., 0] - y) / (2 * (1 - kr))
    s = 1 << (bits - 8)
    planes = np.stack([16 + 219 * y, 128 + 224 * cb, 128 + 224 * cr]) * s
    planes = np.round(planes).clip(0, (1 << bits) - 1).astype(np.uint16 if bits > 8 else np.uint8)
    pix_in = "yuv444p10le" if bits > 8 else "yuv444p"
    pix_out = "yuv420p10le" if bits > 8 else "yuv420p"
    subprocess.run([FFMPEG, "-y", "-v", "error", "-f", "rawvideo", "-pix_fmt", pix_in, "-s", f"{W}x{H}",
                    "-r", "30", *tags, "-i", "-", *tags, "-pix_fmt", pix_out, *encoder, f"{OUT}/{name}"],
                   input=planes.tobytes() * 3, check=True)


def tags(trc, primaries, matrix):
    return ["-color_primaries", primaries, "-color_trc", trc, "-colorspace", matrix, "-color_range", "tv"]


nits = chart_nits()
# Near-lossless standard Main10 (as cameras produce; some hardware decoders
# reject lossless HEVC).
x265 = ["-c:v", "libx265", "-x265-params", "crf=1:log-level=error", "-tag:v", "hvc1"]
write("color-chart-sdr-bt709.mp4", np.clip(nits / WHITE, 0, 1) ** (1 / 2.4), K709, 8,
      tags("bt709", "bt709", "bt709"), ["-c:v", "libx264", "-profile:v", "high", "-crf", "1"])
write("color-chart-hlg.mp4", hlg(nits @ M709_TO_2020.T), K2020, 10,
      tags("arib-std-b67", "bt2020", "bt2020nc"), x265)
write("color-chart-pq.mp4", pq(nits @ M709_TO_2020.T), K2020, 10,
      tags("smpte2084", "bt2020", "bt2020nc"), x265)
