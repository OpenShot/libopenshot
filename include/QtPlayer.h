/**
 * @file
 * @brief Header file for QtPlayer class
 * @author Jonathan Thomas <jonathan@openshot.org>
 *
 * @section LICENSE
 *
 * Copyright (c) 2008-2013 OpenShot Studios, LLC
 * (http://www.openshotstudios.com). This file is part of
 * OpenShot Library (http://www.openshot.org), an open-source project
 * dedicated to delivering high quality video editing and animation solutions
 * to the world.
 *
 * OpenShot Library is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * OpenShot Library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with OpenShot Library.  If not, see <http://www.gnu.org/licenses/>.
 */

#ifndef OPENSHOT_PLAYER_H
#define OPENSHOT_PLAYER_H

#include <iostream>
#include <vector>
#include "../include/ReaderBase.h"

#define _SDL_main_h // This prevents SDL_main from replacing our main() function.
#include <SDL.h>
#include <SDL_thread.h>

using namespace std;

namespace openshot
{
	/// This type is a pointer to a method, used to render the current frame of video on a player.
	typedef void (*CallbackPtr)(int, int, int, const Magick::PixelPacket *Pixels, void *);

	/**
	 * @brief This class is used to playback a video from a reader.
	 *
	 * This player does not actually show the video, but rather it invokes a method each time
	 * a frame should be displayed.  This allows the calling application to display the image using
	 * any toolkit it wishes.
	 */
	class Player
	{
	private:
		ReaderBase *reader;
		CallbackPtr callback;
		void *pythonmethod;

		// draw pixels for SDL
		void putpixel(SDL_Surface *surface, SDL_Overlay *bmp, int x, int y, Uint32 pixel, int pixel_index);

	public:
		/// Default constructor
		Player();

		/// Set the current reader, such as a FFmpegReader
		void SetReader(ReaderBase *p_reader);

		/// Set a callback function which will be invoked each time a frame is ready to be displayed
		void SetFrameCallback(CallbackPtr p_callback, void *p_pythonmethod);

		/// Manually invoke function (if any)
		void Push();

		/// Play the video
		void Play();

	};

}

#endif
