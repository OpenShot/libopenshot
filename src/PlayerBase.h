/**
 * @file
 * @brief Header file for PlayerBase class
 * @author Jonathan Thomas <jonathan@openshot.org>
 *
 * @ref License
 */

// Copyright (c) 2008-2019 OpenShot Studios, LLC
//
// SPDX-License-Identifier: LGPL-3.0-or-later

#ifndef OPENSHOT_PLAYER_BASE_H
#define OPENSHOT_PLAYER_BASE_H

#include <iostream>
#include "ReaderBase.h"

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
		openshot::ReaderBase *reader;
		PlaybackMode mode;

	public:

		/// Display a loading animation
		virtual void Loading() = 0;

		/// Get the current mode
		virtual PlaybackMode Mode() = 0;

		/// Play the video
		virtual void Play() = 0;

		/// Pause the video
		virtual void Pause() = 0;

		/// Get the current frame number being played
		virtual int64_t Position() = 0;

		/// Seek to a specific frame in the player
		virtual void Seek(int64_t new_frame) = 0;

		/// Get the Playback speed
		virtual float Speed() = 0;

		/// Set the Playback speed (1.0 = normal speed, <1.0 = slower, >1.0 faster)
		virtual void Speed(float new_speed) = 0;

		/// Stop the video player and clear the cached frames
		virtual void Stop() = 0;

		/// Get the current reader, such as a FFmpegReader
		virtual openshot::ReaderBase* Reader() = 0;

		/// Set the current reader, such as a FFmpegReader
		virtual void Reader(openshot::ReaderBase *new_reader) = 0;

		/// Get the Volume
		virtual float Volume() = 0;

		/// Set the Volume (1.0 = normal volume, <1.0 = quieter, >1.0 louder)
		virtual void Volume(float new_volume) = 0;

		virtual ~PlayerBase() = default;
	};

}

#endif
