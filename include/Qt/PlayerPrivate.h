/**
 * @file
 * @brief Source file for PlayerPrivate class
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
