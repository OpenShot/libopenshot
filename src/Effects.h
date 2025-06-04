#ifndef OPENSHOT_EFFECTS_H
#define OPENSHOT_EFFECTS_H

/**
 * @file
 * @brief This header includes all commonly used effects for libopenshot, for ease-of-use.
 * @author Jonathan Thomas <jonathan@openshot.org>
 *
 * @ref License
 */

// Copyright (c) 2008-2019 OpenShot Studios, LLC
//
// SPDX-License-Identifier: LGPL-3.0-or-later

/* Effects */
#include "effects/Bars.h"
#include "effects/Blur.h"
#include "effects/Brightness.h"
#include "effects/Caption.h"
#include "effects/ChromaKey.h"
#include "effects/ColorMap.h"
#include "effects/ColorShift.h"
#include "effects/Crop.h"
#include "effects/Deinterlace.h"
#include "effects/Hue.h"
#include "effects/LensFlare.h"
#include "effects/Mask.h"
#include "effects/Negate.h"
#include "effects/Pixelate.h"
#include "effects/Saturation.h"
#include "effects/Sharpen.h"
#include "effects/SphericalProjection.h"
#include "effects/Shift.h"
#include "effects/Wave.h"

/* Audio Effects */
#include "audio_effects/Noise.h"
#include "audio_effects/Delay.h"
#include "audio_effects/Echo.h"
#include "audio_effects/Distortion.h"
#include "audio_effects/ParametricEQ.h"
#include "audio_effects/Compressor.h"
#include "audio_effects/Expander.h"
#include "audio_effects/Robotization.h"
#include "audio_effects/Whisperization.h"

/* OpenCV Effects */
#ifdef USE_OPENCV
#include "effects/Outline.h"
#include "effects/ObjectDetection.h"
#include "effects/Tracker.h"
#include "effects/Stabilizer.h"
#endif



#endif
