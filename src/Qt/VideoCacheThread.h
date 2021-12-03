/**
 * @file
 * @brief Source file for VideoCacheThread class
 * @author Jonathan Thomas <jonathan@openshot.org>
 *
 * @ref License
 */

// Copyright (c) 2008-2019 OpenShot Studios, LLC
//
// SPDX-License-Identifier: LGPL-3.0-or-later

#ifndef OPENSHOT_VIDEO_CACHE_THREAD_H
#define OPENSHOT_VIDEO_CACHE_THREAD_H

#include "ReaderBase.h"

#include <AppConfig.h>
#include <juce_audio_basics/juce_audio_basics.h>

namespace openshot
{
    using juce::Thread;
    using juce::WaitableEvent;

    /**
     *  @brief The video cache class.
     */
    class VideoCacheThread : Thread
    {
	private:
	std::atomic_int position;

	protected:
	std::shared_ptr<Frame> frame;
	int speed;
	bool is_playing;
	int64_t current_display_frame;
	ReaderBase *reader;
	int max_concurrent_frames;

	/// Constructor
	VideoCacheThread();
	/// Destructor
	~VideoCacheThread();

	/// Get the currently playing frame number (if any)
	int64_t getCurrentFramePosition();

    /// Get Speed (The speed and direction to playback a reader (1=normal, 2=fast, 3=faster, -1=rewind, etc...)
    int getSpeed() const { return speed; }

	/// Play the video
	void Play();

	/// Seek the reader to a particular frame number
	void Seek(int64_t new_position);

	/// Set the currently displaying frame number
	void setCurrentFramePosition(int64_t current_frame_number);

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
