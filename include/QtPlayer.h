/**
 * @file
 * @brief Header file for QtPlayer class
 * @author Duzy Chan <code@duzy.info>
 * @author Jonathan Thomas <jonathan@openshot.org>
 *
 * @ref License
 */

/* LICENSE
 *
 * Copyright (c) 2008-2019 OpenShot Studios, LLC
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

#ifndef OPENSHOT_QT_PLAYER_H
#define OPENSHOT_QT_PLAYER_H

#include <iostream>
#include <vector>
#include "PlayerBase.h"
#include "Qt/PlayerPrivate.h"
#include "RendererBase.h"

namespace openshot
{
    /**
     * @brief This class is used to playback a video from a reader.
     *
     */
    class QtPlayer : public openshot::PlayerBase
    {
	openshot::PlayerPrivate *p;
	bool threads_started;

    public:
	/// Default constructor
	explicit QtPlayer();
	explicit QtPlayer(openshot::RendererBase *rb);

	/// Default destructor
	virtual ~QtPlayer();

	/// Close audio device
	void CloseAudioDevice();

	/// Get Error (if any)
	std::string GetError();

	/// Get Audio Devices from JUCE
	std::vector<openshot::AudioDeviceInfo> GetAudioDeviceNames();

	/// Play the video
	void Play();

	/// Display a loading animation
	void Loading();

	/// Get the current mode
	openshot::PlaybackMode Mode();

	/// Pause the video
	void Pause();

	/// Get the current frame number being played
	int64_t Position();

  /// Seek to a specific frame in the player
	void Seek(int64_t new_frame);

	/// Set the source URL/path of this player (which will create an internal Reader)
	void SetSource(const std::string &source);

	/// Set the QWidget which will be used as the display (note: QLabel works well). This does not take a
	/// normal pointer, but rather a LONG pointer id (and it re-casts the QWidget pointer inside libopenshot).
	/// This is required due to SIP and SWIG incompatibility in the Python bindings.
	void SetQWidget(int64_t qwidget_address);

	/// Get the Renderer pointer address (for Python to cast back into a QObject)
	int64_t GetRendererQObject();

	/// Get the Playback speed
	float Speed();

	/// Set the Playback speed (1.0 = normal speed, <1.0 = slower, >1.0 faster)
	void Speed(float new_speed);

	/// Stop the video player and clear the cached frames
	void Stop();

	/// Set the current reader
	void Reader(openshot::ReaderBase *new_reader);

	/// Get the current reader, such as a FFmpegReader
	openshot::ReaderBase* Reader();

	/// Get the Volume
	float Volume();

	/// Set the Volume (1.0 = normal volume, <1.0 = quieter, >1.0 louder)
	void Volume(float new_volume);
    };
}

#endif
