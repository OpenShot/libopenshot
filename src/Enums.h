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

/// This enumeration determines how clips are blended with lower layers
enum BlendMode
{
    BLEND_SOURCE_OVER,      ///< This is the default mode. The alpha of the current clip is used to blend the pixel on top of the lower layer.
    BLEND_DESTINATION_OVER, ///< The alpha of the lower layer is used to blend it on top of the current clip pixels. This mode is the inverse of BLEND_SOURCEOVER.
    BLEND_CLEAR,            ///< The pixels in the lower layer are cleared (set to fully transparent) independent of the current clip.
    BLEND_SOURCE,           ///< The output is the current clip pixel. (This means a basic copy operation and is identical to SourceOver when the current clip pixel is opaque).
    BLEND_DESTINATION,      ///< The output is the lower layer pixel. This means that the blending has no effect. This mode is the inverse of BLEND_SOURCE.
    BLEND_SOURCE_IN,        ///< The output is the current clip,where the alpha is reduced by that of the lower layer.
    BLEND_DESTINATION_IN,   ///< The output is the lower layer,where the alpha is reduced by that of the current clip.  This mode is the inverse of BLEND_SOURCEIN.
    BLEND_SOURCE_OUT,       ///< The output is the current clip,where the alpha is reduced by the inverse of lower layer.
    BLEND_DESTINATION_OUT,  ///< The output is the lower layer,where the alpha is reduced by the inverse of the current clip. This mode is the inverse of BLEND_SOURCEOUT.
    BLEND_SOURCE_ATOP,      ///< The current clip pixel is blended on top of the lower layer,with the alpha of the current clip pixel reduced by the alpha of the lower layer pixel.
    BLEND_DESTINATION_ATOP, ///< The lower layer pixel is blended on top of the current clip,with the alpha of the lower layer pixel is reduced by the alpha of the lower layer pixel. This mode is the inverse of BLEND_SOURCEATOP.
    BLEND_XOR,              ///< The current clip,whose alpha is reduced with the inverse of the lower layer alpha,is merged with the lower layer,whose alpha is reduced by the inverse of the current clip alpha. BLEND_XOR is not the same as the bitwise Xor.
    BLEND_PLUS,             ///< Both the alpha and color of the current clip and lower layer pixels are added together.
    BLEND_MULTIPLY,         ///< The output is the current clip color multiplied by the lower layer. Multiplying a color with white leaves the color unchanged,while multiplying a color with black produces black.
    BLEND_SCREEN,           ///< The current clip and lower layer colors are inverted and then multiplied. Screening a color with white produces white,whereas screening a color with black leaves the color unchanged.
    BLEND_OVERLAY,          ///< Multiplies or screens the colors depending on the lower layer color. The lower layer color is mixed with the current clip color to reflect the lightness or darkness of the lower layer.
    BLEND_DARKEN,           ///< The darker of the current clip and lower layer colors is selected.
    BLEND_LIGHTEN,          ///< The lighter of the current clip and lower layer colors is selected.
    BLEND_COLOR_DODGE,      ///< The lower layer color is brightened to reflect the current clip color. A black current clip color leaves the lower layer color unchanged.
    BLEND_COLOR_BURN,       ///< The lower layer color is darkened to reflect the current clip color. A white current clip color leaves the lower layer color unchanged.
    BLEND_HARD_LIGHT,       ///< Multiplies or screens the colors depending on the current clip color. A light current clip color will lighten the lower layer color,whereas a dark current clip color will darken the lower layer color.
    BLEND_SOFT_LIGHT,       ///< Darkens or lightens the colors depending on the current clip color. Similar to BLEND_HARDLIGHT.
    BLEND_DIFFERENCE,       ///< Subtracts the darker of the colors from the lighter. Painting with white inverts the lower layer color,whereas painting with black leaves the lower layer color unchanged.
    BLEND_EXCLUSION         ///< Similar to BLEND_DIFFERENCE,but with a lower contrast. Painting with white inverts the lower layer color,whereas painting with black leaves the lower layer color unchanged.
};

}  // namespace openshot

#endif
