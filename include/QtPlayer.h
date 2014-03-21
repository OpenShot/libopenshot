/**
 * @file
 * @brief Header file for QtPlayer class
 * @author Duzy Chan <code@duzy.info>
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

#ifndef OPENSHOT_QT_PLAYER_H
#define OPENSHOT_QT_PLAYER_H

#include <iostream>
#include <vector>
#include "../include/PlayerBase.h"

using namespace std;

namespace openshot
{
    class RendererBase;
    class PlayerPrivate;

    /**
     * @brief This class is used to playback a video from a reader.
     *
     */
    class QtPlayer : public PlayerBase
    {
	PlayerPrivate *p;

    public:
	/// Default constructor
	explicit QtPlayer(RendererBase *rb);

	/// Default destructor
	virtual ~QtPlayer();

	/// Set the source URL/path of this player (which will create an internal Reader)
	void SetSource(const std::string &source);
	
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
