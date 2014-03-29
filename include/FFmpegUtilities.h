/**
 * @file
 * @brief Header file for FFmpegUtilities
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
 * and/or modify it under the terms of the GNU Affero General Public License
 * as published by the Free Software Foundation, either version 3 of the
 * License, or (at your option) any later version.
 *
 * OpenShot Library (libopenshot) is distributed in the hope that it will be
 * useful, but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU Affero General Public License for more details.
 *
 * You should have received a copy of the GNU Affero General Public License
 * along with OpenShot Library. If not, see <http://www.gnu.org/licenses/>.
 *
 * Also, if your software can interact with users remotely through a computer
 * network, you should also make sure that it provides a way for users to
 * get its source. For example, if your program is a web application, its
 * interface could display a "Source" link that leads users to an archive
 * of the code. There are many ways you could offer source, and different
 * solutions will be better for different programs; see section 13 for the
 * specific requirements.
 *
 * You should also get your employer (if you work as a programmer) or school,
 * if any, to sign a "copyright disclaimer" for the program, if necessary.
 * For more information on this, and how to apply and follow the GNU AGPL, see
 * <http://www.gnu.org/licenses/>.
 */

#ifndef OPENSHOT_FFMPEG_UTILITIES_H
#define OPENSHOT_FFMPEG_UTILITIES_H

	// Required for libavformat to build on Windows
	#ifndef INT64_C
	#define INT64_C(c) (c ## LL)
	#define UINT64_C(c) (c ## ULL)
	#endif

	// Include the FFmpeg headers
	extern "C" {
		#include <libavcodec/avcodec.h>
		#include <libavformat/avformat.h>
		#include <libswscale/swscale.h>
		#include <libavutil/mathematics.h>
		#include <libavutil/pixfmt.h>
		#include <libavutil/pixdesc.h>

		// libavutil changed folders at some point
		#if LIBAVFORMAT_VERSION_MAJOR >= 53
			#include <libavutil/opt.h>
		#else
			#include <libavcodec/opt.h>
		#endif

	}

	// This was removed from newer versions of FFmpeg (but still used in libopenshot)
	#ifndef AVCODEC_MAX_AUDIO_FRAME_SIZE
		#define AVCODEC_MAX_AUDIO_FRAME_SIZE 192000 // 1 second of 48khz 32bit audio
	#endif
	#ifndef AV_ERROR_MAX_STRING_SIZE
		#define AV_ERROR_MAX_STRING_SIZE 64
	#endif

	// This wraps an unsafe C macro to be C++ compatible function
	static const std::string av_make_error_string(int errnum)
	{
		char errbuf[AV_ERROR_MAX_STRING_SIZE];
		av_strerror(errnum, errbuf, AV_ERROR_MAX_STRING_SIZE);
		return (std::string)errbuf;
	}

	// Redefine the C macro to use our new C++ function
	#undef av_err2str
	#define av_err2str(errnum) av_make_error_string(errnum).c_str()

#endif
