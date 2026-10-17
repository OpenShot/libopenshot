// SPDX-FileCopyrightText: 2026 OpenShot Studios, LLC
// SPDX-License-Identifier: LGPL-3.0-or-later

#ifndef OPENSHOT_COLORSPACE_H
#define OPENSHOT_COLORSPACE_H

namespace openshot {

enum class RGBColorSpace {
    BT709,
    BT2020,
    DisplayP3
};

// Original source-video color characteristics.
// Values correspond to FFmpeg/ITU color metadata identifiers.
// These describe the encoded source, not necessarily converted RGB pixels.
struct ColorMetadata {
    int primaries = 2;
    int transfer = 2;
    int matrix = 2;
    int range = 0;
    int chroma_location = 0;
};

// Transfer characteristics for encoded RGB signals.
enum class TransferFunction {
    BT709,
    SRGB,
    PQ,
    HLG
};

// Convert one encoded component to linear light.
// PQ returns absolute luminance in cd/m^2 (nits).
// Other functions return relative linear-light values.
double DecodeTransfer(double encoded, TransferFunction function);

// Convert one linear-light component to an encoded signal.
// PQ expects absolute luminance in cd/m^2 (nits).
// Other functions expect relative linear-light values.
double EncodeTransfer(double linear, TransferFunction function);

struct LinearRGB {
    double r;
    double g;
    double b;
};

// Convert linear-light RGB between D65 color spaces.
// Does not decode transfer functions, tone-map, or clip.
LinearRGB ConvertLinearRGB(
    const LinearRGB& rgb,
    RGBColorSpace source,
    RGBColorSpace destination);

} // namespace openshot

#endif
