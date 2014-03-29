/**
 * @file
 * @brief Header file for SDLPlayer class
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
