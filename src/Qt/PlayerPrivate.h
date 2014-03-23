/**
 * @file
 * @brief Source file for PlayerPrivate class
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

#include "../include/ReaderBase.h"
#include "../include/RendererBase.h"
#include "../include/AudioReaderSource.h"
#include "AudioPlaybackThread.h"
#include "VideoPlaybackThread.h"

namespace openshot
{
    class AudioPlaybackThread;
    class VideoPlaybackThread;
    using juce::Thread;

    /**
     *  @brief The private part of QtPlayer class, which contains an audio thread and video thread,
     *  and controls the video timing and audio synchronization code.
     */
    class PlayerPrivate : Thread
    {
	int video_position; /// The current frame position.
	int audio_position; /// The current frame position.
	ReaderBase *reader; /// The reader which powers this player
	AudioPlaybackThread *audioPlayback; /// The audio thread
	VideoPlaybackThread *videoPlayback; /// The video thread
	int speed; /// The speed and direction to playback a reader (1=normal, 2=fast, 3=faster, -1=rewind, etc...)

	/// Constructor
	PlayerPrivate(RendererBase *rb);
	/// Destructor
	virtual ~PlayerPrivate();

	/// Start thread
	void run();

    /// Seek to a frame
    void Seek(int new_position);

	/// Start the video/audio playback
	bool startPlayback();

	/// Stop the video/audio playback
	void stopPlayback(int timeOutMilliseconds = -1);

	/// Get the next frame (based on speed and direction)
	tr1::shared_ptr<Frame> getFrame();

	/// Set Speed (The speed and direction to playback a reader (1=normal, 2=fast, 3=faster, -1=rewind, etc...)
	void Speed(int new_speed);
	/// Get Speed (The speed and direction to playback a reader (1=normal, 2=fast, 3=faster, -1=rewind, etc...)
	int Speed() const { return speed; }

	/// Set the current reader
	void Reader(ReaderBase *new_reader);

	/// The parent class of PlayerPrivate
	friend class QtPlayer;
    };

}
