/**
 * @file
 * @brief Shared helpers for Crop effect scaling logic
 * @author Jonathan Thomas <jonathan@openshot.org>
 *
 * @ref License
 */

// Copyright (c) 2008-2025 OpenShot Studios, LLC
//
// SPDX-License-Identifier: LGPL-3.0-or-later

#ifndef OPENSHOT_CROP_HELPERS_H
#define OPENSHOT_CROP_HELPERS_H

namespace openshot {

class Clip;
class Crop;

/// Return the first Crop effect on this clip that has resize enabled (if any)
const Crop* FindResizingCropEffect(Clip* clip);

/// Scale the requested max_width / max_height based on the Crop resize amount, capped by source size
void ApplyCropResizeScale(Clip* clip, int source_width, int source_height, int& max_width, int& max_height);

} // namespace openshot

#endif // OPENSHOT_CROP_HELPERS_H
