/**
 * @file
 * @brief Header file for PlayerBase class
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

#ifndef OPENSHOT_PLAYER_BASE_H
#define OPENSHOT_PLAYER_BASE_H

#include <iostream>
#include "../include/ReaderBase.h"

using namespace std;

namespace openshot
{
	/**
	 * @brief This enumeration determines the mode of the video player (i.e. playing, paused, etc...)
	 *
	 * A player can be in one of the following modes, which controls how it behaves.
	 */
	enum PlaybackMode
	{
		PLAYBACK_PLAY,		///< Play the video normally
		PLAYBACK_PAUSED,	///< Pause the video (holding the last displayed frame)
		PLAYBACK_LOADING,	///< Loading the video (display a loading animation)
		PLAYBACK_STOPPED,	///< Stop playing the video (clear cache, done with player)
	};

	/**
	 * @brief This is the base class of all Players in libopenshot.
	 *
	 * Players are responsible for displaying images and playing back audio samples with specific
	 * frame rates and sample rates. All Players must be based on this class, and include these
	 * methods.
	 */
	class PlayerBase
	{
	protected:
		float speed;
		float volume;
		ReaderBase *reader;
		PlaybackMode mode;

	public:

		/// Display a loading animation
		virtual void Loading() = 0;

		/// Play the video
		virtual void Play() = 0;

		/// Pause the video
		virtual void Pause() = 0;

		/// Get the current frame number being played
		virtual int Position() = 0;

		/// Seek to a specific frame in the player
		virtual void Seek(int new_frame) = 0;

		/// Get the Playback speed
		float Speed();

		/// Set the Playback speed (1.0 = normal speed, <1.0 = slower, >1.0 faster)
		void Speed(float new_speed);

		/// Stop the video player and clear the cached frames
		virtual void Stop() = 0;

		/// Get the current reader, such as a FFmpegReader
		ReaderBase* Reader();

		/// Set the current reader, such as a FFmpegReader
		void Reader(ReaderBase *new_reader);

		/// Get the Volume
		float Volume();

		/// Set the Volume (1.0 = normal volume, <1.0 = quieter, >1.0 louder)
		void Volume(float new_volume);

	};

}

#endif
