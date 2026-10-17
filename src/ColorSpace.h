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
