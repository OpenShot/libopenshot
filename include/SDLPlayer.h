/**
 * @file
 * @brief Header file for SDLPlayer class
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

#ifndef OPENSHOT_SDL_PLAYER_H
#define OPENSHOT_SDL_PLAYER_H

#include <iostream>
#include <vector>
#include "../include/PlayerBase.h"
#include "../include/ReaderBase.h"

#define _SDL_main_h // This prevents SDL_main from replacing our main() function.
#include <SDL.h>
#include <SDL_thread.h>

using namespace std;

namespace openshot
{
	/**
	 * @brief Player to display a video using SDL (Simple DirectMedia Layer)
	 *
	 * This player uses the SDL (Simple DirectMedia Layer) library to display the video. It uses
	 * an image overlay with YUV420 colorspace, and draws the video to any X11 window you specify.
	 */
	class SDLPlayer : public PlayerBase
	{
	private:
		int position; ///< Current frame number being played

	public:
		/// Default constructor
		SDLPlayer();

		/// Play the video
		void Play();

		/// Display a loading animation
		void Loading();

		/// Pause the video
		void Pause();

		/// Get the current frame number being played
		int Position();

		/// Seek to a specific frame in the player
		void Seek(int new_frame);

		/// Stop the video player and clear the cached frames
		void Stop();

	};

}

#endif
