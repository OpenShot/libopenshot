/**
 * @file
 * @brief Header file for TextReader class
 * @author Jonathan Thomas <jonathan@openshot.org>
 *
 * @ref License
 */

// Copyright (c) 2008-2019 OpenShot Studios, LLC
//
// SPDX-License-Identifier: LGPL-3.0-or-later

#ifndef OPENSHOT_ENUMS_H
#define OPENSHOT_ENUMS_H


namespace openshot
{

/// This enumeration determines how clips are aligned to their parent container.
enum GravityType
{
	GRAVITY_TOP_LEFT,		///< Align clip to the top left of its parent
	GRAVITY_TOP,			///< Align clip to the top center of its parent
	GRAVITY_TOP_RIGHT,		///< Align clip to the top right of its parent
	GRAVITY_LEFT,			///< Align clip to the left of its parent (middle aligned)
	GRAVITY_CENTER,			///< Align clip to the center of its parent (middle aligned)
	GRAVITY_RIGHT,			///< Align clip to the right of its parent (middle aligned)
	GRAVITY_BOTTOM_LEFT,	///< Align clip to the bottom left of its parent
	GRAVITY_BOTTOM,			///< Align clip to the bottom center of its parent
	GRAVITY_BOTTOM_RIGHT	///< Align clip to the bottom right of its parent
};

/// This enumeration determines how clips are scaled to fit their parent container.
enum ScaleType
{
	SCALE_CROP,		///< Scale the clip until both height and width fill the canvas (cropping the overlap)
	SCALE_FIT,		///< Scale the clip until either height or width fills the canvas (with no cropping)
	SCALE_STRETCH,	///< Scale the clip until both height and width fill the canvas (distort to fit)
	SCALE_NONE		///< Do not scale the clip
};

/// This enumeration determines what parent a clip should be aligned to.
enum AnchorType
{
	ANCHOR_CANVAS,	///< Anchor the clip to the canvas
	ANCHOR_VIEWPORT	///< Anchor the clip to the viewport (which can be moved / animated around the canvas)
};

/// This enumeration determines the display format of the clip's frame number (if any). Useful for debugging.
enum FrameDisplayType
{
	FRAME_DISPLAY_NONE,     ///< Do not display the frame number
	FRAME_DISPLAY_CLIP,     ///< Display the clip's internal frame number
	FRAME_DISPLAY_TIMELINE, ///< Display the timeline's frame number
	FRAME_DISPLAY_BOTH      ///< Display both the clip's and timeline's frame number
};

/// This enumeration determines the strategy when mixing audio with other clips.
enum VolumeMixType
{
	VOLUME_MIX_NONE,   	///< Do not apply any volume mixing adjustments. Just add the samples together.
	VOLUME_MIX_AVERAGE,	///< Evenly divide the overlapping clips volume keyframes, so that the sum does not exceed 100%
	VOLUME_MIX_REDUCE 	///< Reduce volume by about %25, and then mix (louder, but could cause pops if the sum exceeds 100%)
};


/// This enumeration determines the distortion type of Distortion Effect.
enum DistortionType
{
	HARD_CLIPPING,
	SOFT_CLIPPING,
	EXPONENTIAL,
	FULL_WAVE_RECTIFIER,
	HALF_WAVE_RECTIFIER,
};

/// This enumeration determines the filter type of ParametricEQ Effect.
enum FilterType
{
	LOW_PASS,
	HIGH_PASS,
	LOW_SHELF,
	HIGH_SHELF,
	BAND_PASS,
	BAND_STOP,
	PEAKING_NOTCH,
};

/// This enumeration determines the FFT size.
enum FFTSize
{
	FFT_SIZE_32,
	FFT_SIZE_64,
	FFT_SIZE_128,
	FFT_SIZE_256,
	FFT_SIZE_512,
	FFT_SIZE_1024,
	FFT_SIZE_2048,
	FFT_SIZE_4096,
	FFT_SIZE_8192,
};

/// This enumeration determines the hop size.
enum HopSize {
    HOP_SIZE_2,
    HOP_SIZE_4,
    HOP_SIZE_8,
};

/// This enumeration determines the window type.
enum WindowType {
    RECTANGULAR,
    BART_LETT,
    HANN,
    HAMMING,
};

/// This enumeration determines the algorithm used by the ChromaKey filter
enum ChromaKeyMethod
{
    CHROMAKEY_BASIC,        ///< Length of difference between RGB vectors
    CHROMAKEY_HSVL_H,       ///< Difference between HSV/HSL hues
    CHROMAKEY_HSV_S,        ///< Difference between HSV saturations
    CHROMAKEY_HSL_S,        ///< Difference between HSL saturations
    CHROMAKEY_HSV_V,        ///< Difference between HSV values
    CHROMAKEY_HSL_L,        ///< Difference between HSL luminances
    CHROMAKEY_CIE_LCH_L,    ///< Difference between CIE LCH(ab) luminousities
    CHROMAKEY_CIE_LCH_C,    ///< Difference between CIE LCH(ab) chromas
    CHROMAKEY_CIE_LCH_H,    ///< Difference between CIE LCH(ab) hues
    CHROMAKEY_CIE_DISTANCE, ///< CIEDE2000 perceptual difference
    CHROMAKEY_YCBCR,        ///< YCbCr vector difference of CbCr
    CHROMAKEY_LAST_METHOD = CHROMAKEY_YCBCR
};

}  // namespace openshot

#endif
