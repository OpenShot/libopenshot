/**
 * @file
 * @brief Source file for PlayerPrivate class
 * @author Duzy Chan <code@duzy.info>
 * @author Jonathan Thomas <jonathan@openshot.org>
 *
 * @ref License
 */

// Copyright (c) 2008-2019 OpenShot Studios, LLC
//
// SPDX-License-Identifier: LGPL-3.0-or-later

#ifndef OPENSHOT_PLAYER_PRIVATE_H
#define OPENSHOT_PLAYER_PRIVATE_H

#include "../ReaderBase.h"
#include "../RendererBase.h"
#include "../AudioReaderSource.h"
#include "../Qt/AudioPlaybackThread.h"
#include "../Qt/VideoPlaybackThread.h"
#include "../Qt/VideoCacheThread.h"

namespace openshot
{
    /**
     *  @brief The private part of QtPlayer class, which contains an audio thread and video thread,
     *  and controls the video timing and audio synchronization code.
     */
    class PlayerPrivate : juce::Thread
    {
    std::shared_ptr<openshot::Frame> frame; /// The current frame
	int64_t video_position; /// The current frame position.
	int64_t audio_position; /// The current frame position.
	openshot::ReaderBase *reader; /// The reader which powers this player
	openshot::AudioPlaybackThread *audioPlayback; /// The audio thread
	openshot::VideoPlaybackThread *videoPlayback; /// The video thread
	openshot::VideoCacheThread *videoCache; /// The cache thread
	int speed; /// The speed and direction to playback a reader (1=normal, 2=fast, 3=faster, -1=rewind, etc...)
	openshot::RendererBase *renderer;
	int64_t last_video_position; /// The last frame actually displayed

	/// Constructor
	PlayerPrivate(openshot::RendererBase *rb);
	/// Destructor
	virtual ~PlayerPrivate();

	/// Start thread
	void run();

	/// Start the video/audio playback
	bool startPlayback();

	/// Stop the video/audio playback
	void stopPlayback(int timeOutMilliseconds = -1);

	/// Get the next frame (based on speed and direction)
	std::shared_ptr<openshot::Frame> getFrame();

	/// The parent class of PlayerPrivate
	friend class QtPlayer;
    };

}

#endif // OPENSHOT_PLAYER_PRIVATE_H
