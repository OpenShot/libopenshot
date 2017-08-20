/**
 * @file
 * @brief Source file for PlayerPrivate class
 * @author Duzy Chan <code@duzy.info>
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

#include "../../include/ReaderBase.h"
#include "../../include/RendererBase.h"
#include "../../include/AudioReaderSource.h"
#include "../../include/Qt/AudioPlaybackThread.h"
#include "../../include/Qt/VideoPlaybackThread.h"
#include "../../include/Qt/VideoCacheThread.h"

namespace openshot
{
    using juce::Thread;

    /**
     *  @brief The private part of QtPlayer class, which contains an audio thread and video thread,
     *  and controls the video timing and audio synchronization code.
     */
    class PlayerPrivate : Thread
    {
    std::shared_ptr<Frame> frame; /// The current frame
	long int video_position; /// The current frame position.
	long int audio_position; /// The current frame position.
	ReaderBase *reader; /// The reader which powers this player
	AudioPlaybackThread *audioPlayback; /// The audio thread
	VideoPlaybackThread *videoPlayback; /// The video thread
	VideoCacheThread *videoCache; /// The cache thread
	int speed; /// The speed and direction to playback a reader (1=normal, 2=fast, 3=faster, -1=rewind, etc...)
	RendererBase *renderer;
	long int last_video_position; /// The last frame actually displayed

	/// Constructor
	PlayerPrivate(RendererBase *rb);
	/// Destructor
	virtual ~PlayerPrivate();

	/// Start thread
	void run();

	/// Start the video/audio playback
	bool startPlayback();

	/// Stop the video/audio playback
	void stopPlayback(int timeOutMilliseconds = -1);

	/// Get the next frame (based on speed and direction)
	std::shared_ptr<Frame> getFrame();

	/// The parent class of PlayerPrivate
	friend class QtPlayer;
    };

}

#endif // OPENSHOT_PLAYER_PRIVATE_H
