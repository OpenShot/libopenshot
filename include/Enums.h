/**
 * @file
 * @brief Header file for TextReader class
 * @author Jonathan Thomas <jonathan@openshot.org>
 *
 * @section LICENSE
 *
 * Copyright (c) 2008-2014 OpenShot Studios, LLC
 * <http://www.openshotstudios.com/>. This file is part of
 * OpenShot Library (libopenshot), an open-source project dedicated to
 * delivering high quality video editing and animation solutions to the
 * world. For more information visit <http://www.openshot.org/>.
 *
 * OpenShot Library (libopenshot) is free software: you can redistribute it
 * and/or modify it under the terms of the GNU Lesser General Public License
 * as published by the Free Software Foundation, either version 3 of the
 * License, or (at your option) any later version.
 *
 * OpenShot Library (libopenshot) is distributed in the hope that it will be
 * useful, but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public License
 * along with OpenShot Library. If not, see <http://www.gnu.org/licenses/>.
 */

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
}
#endif
