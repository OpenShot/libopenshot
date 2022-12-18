/**
 * @file
 * @brief Header file for QtPlayer class
 * @author Duzy Chan <code@duzy.info>
 * @author Jonathan Thomas <jonathan@openshot.org>
 *
 * @ref License
 */

// Copyright (c) 2008-2019 OpenShot Studios, LLC
//
// SPDX-License-Identifier: LGPL-3.0-or-later

#ifndef OPENSHOT_QT_PLAYER_H
#define OPENSHOT_QT_PLAYER_H

#include <iostream>
#include <vector>

#include "PlayerBase.h"
#include "Qt/PlayerPrivate.h"
#include "RendererBase.h"

namespace openshot
{
	using AudioDeviceList = std::vector<std::pair<std::string, std::string>>;

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

	/// Return the default audio sample rate (from the system)
	double GetDefaultSampleRate();

	/// Get Audio Devices from JUCE
	AudioDeviceList GetAudioDeviceNames();

	/// Get current audio device or last attempted
	AudioDeviceInfo GetCurrentAudioDevice();

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

	/// Set the source JSON of an openshot::Timelime
	void SetTimelineSource(const std::string &json);

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
