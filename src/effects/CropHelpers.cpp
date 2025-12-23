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

#include "CropHelpers.h"

#include <algorithm>
#include <cmath>
#include <limits>

#include "../Clip.h"
#include "Crop.h"

namespace openshot {

const Crop* FindResizingCropEffect(Clip* clip) {
	if (!clip) {
		return nullptr;
	}

	for (auto effect : clip->Effects()) {
		if (auto crop_effect = dynamic_cast<Crop*>(effect)) {
			if (crop_effect->resize) {
				return crop_effect;
			}
		}
	}

	return nullptr;
}

void ApplyCropResizeScale(Clip* clip, int source_width, int source_height, int& max_width, int& max_height) {
	const Crop* crop_effect = FindResizingCropEffect(clip);
	if (!crop_effect) {
		return;
	}

	const float max_left = crop_effect->left.GetMaxPoint().co.Y;
	const float max_right = crop_effect->right.GetMaxPoint().co.Y;
	const float max_top = crop_effect->top.GetMaxPoint().co.Y;
	const float max_bottom = crop_effect->bottom.GetMaxPoint().co.Y;

	const float visible_width = std::max(0.01f, 1.0f - max_left - max_right);
	const float visible_height = std::max(0.01f, 1.0f - max_top - max_bottom);

	const double scaled_width = std::ceil(max_width / visible_width);
	const double scaled_height = std::ceil(max_height / visible_height);

	const double clamped_width = std::min<double>(source_width, scaled_width);
	const double clamped_height = std::min<double>(source_height, scaled_height);

	max_width = static_cast<int>(std::min<double>(std::numeric_limits<int>::max(), clamped_width));
	max_height = static_cast<int>(std::min<double>(std::numeric_limits<int>::max(), clamped_height));
}

} // namespace openshot
