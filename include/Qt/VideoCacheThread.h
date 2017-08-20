/**
 * @file
 * @brief Source file for VideoCacheThread class
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

#ifndef OPENSHOT_VIDEO_CACHE_THREAD_H
#define OPENSHOT_VIDEO_CACHE_THREAD_H

#include "../../include/OpenMPUtilities.h"
#include "../../include/ReaderBase.h"
#include "../../include/RendererBase.h"

namespace openshot
{
    using juce::Thread;
    using juce::WaitableEvent;

    /**
     *  @brief The video cache class.
     */
    class VideoCacheThread : Thread
    {
	std::shared_ptr<Frame> frame;
	int speed;
	bool is_playing;
	long int position;
	long int current_display_frame;
	ReaderBase *reader;
	int max_frames;

	/// Constructor
	VideoCacheThread();
	/// Destructor
	~VideoCacheThread();

	/// Get the currently playing frame number (if any)
	long int getCurrentFramePosition();

    /// Get Speed (The speed and direction to playback a reader (1=normal, 2=fast, 3=faster, -1=rewind, etc...)
    int getSpeed() const { return speed; }

	/// Play the video
	void Play();

	/// Seek the reader to a particular frame number
	void Seek(long int new_position);

	/// Set the currently displaying frame number
	void setCurrentFramePosition(long int current_frame_number);

    /// Set Speed (The speed and direction to playback a reader (1=normal, 2=fast, 3=faster, -1=rewind, etc...)
    void setSpeed(int new_speed) { speed = new_speed; }

	/// Stop the audio playback
	void Stop();

	/// Start the thread
	void run();

	/// Set the current thread's reader
	void Reader(ReaderBase *new_reader) { reader=new_reader; Play(); };

	/// Parent class of VideoCacheThread
	friend class PlayerPrivate;
	friend class QtPlayer;
    };

}

#endif // OPENSHOT_VIDEO_CACHE_THREAD_H
