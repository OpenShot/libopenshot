/**
 * @file
 * @brief Header file for ChannelLayout class
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

#ifndef OPENSHOT_CHANNEL_LAYOUT_H
#define OPENSHOT_CHANNEL_LAYOUT_H

// Include FFmpeg headers and macros
#include "FFmpegUtilities.h"

namespace openshot
{

/**
 * @brief This enumeration determines the audio channel layout (such as stereo, mono, 5 point surround, etc...)
 *
 * When writing video and audio files, you will need to specify the channel layout of the audio stream. libopenshot
 * can convert between many different channel layouts (such as stereo, mono, 5 point surround, etc...)
 */
enum ChannelLayout
{
	LAYOUT_MONO = AV_CH_LAYOUT_MONO,
	LAYOUT_STEREO = AV_CH_LAYOUT_STEREO,
	LAYOUT_2POINT1 = AV_CH_LAYOUT_2POINT1,
	LAYOUT_2_1 = AV_CH_LAYOUT_2_1,
	LAYOUT_SURROUND = AV_CH_LAYOUT_SURROUND,
	LAYOUT_3POINT1 = AV_CH_LAYOUT_3POINT1,
	LAYOUT_4POINT0 = AV_CH_LAYOUT_4POINT0,
	LAYOUT_4POINT1 = AV_CH_LAYOUT_4POINT1,
	LAYOUT_2_2 = AV_CH_LAYOUT_2_2,
	LAYOUT_QUAD = AV_CH_LAYOUT_QUAD,
	LAYOUT_5POINT0 = AV_CH_LAYOUT_5POINT0,
	LAYOUT_5POINT1 = AV_CH_LAYOUT_5POINT1,
	LAYOUT_5POINT0_BACK = AV_CH_LAYOUT_5POINT0_BACK,
	LAYOUT_5POINT1_BACK = AV_CH_LAYOUT_5POINT1_BACK,
	LAYOUT_6POINT0 = AV_CH_LAYOUT_6POINT0,
	LAYOUT_6POINT0_FRONT = AV_CH_LAYOUT_6POINT0_FRONT,
	LAYOUT_HEXAGONAL = AV_CH_LAYOUT_HEXAGONAL,
	LAYOUT_6POINT1 = AV_CH_LAYOUT_6POINT1,
	LAYOUT_6POINT1_BACK = AV_CH_LAYOUT_6POINT1_BACK,
	LAYOUT_6POINT1_FRONT = AV_CH_LAYOUT_6POINT1_FRONT,
	LAYOUT_7POINT0 = AV_CH_LAYOUT_7POINT0,
	LAYOUT_7POINT0_FRONT = AV_CH_LAYOUT_7POINT0_FRONT,
	LAYOUT_7POINT1 = AV_CH_LAYOUT_7POINT1,
	LAYOUT_7POINT1_WIDE = AV_CH_LAYOUT_7POINT1_WIDE,
	LAYOUT_7POINT1_WIDE_BACK = AV_CH_LAYOUT_7POINT1_WIDE_BACK,
	LAYOUT_OCTAGONAL = AV_CH_LAYOUT_OCTAGONAL,
	LAYOUT_STEREO_DOWNMIX = AV_CH_LAYOUT_STEREO_DOWNMIX
};



}

#endif
