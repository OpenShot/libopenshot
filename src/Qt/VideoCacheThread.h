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
	protected:
	std::shared_ptr<Frame> last_cached_frame;
	int speed;
	int last_speed;
	bool is_playing;
	int64_t requested_display_frame;
	int64_t current_display_frame;
	int64_t cached_frame_count = 0;
	ReaderBase *reader;
	int64_t min_frames_ahead;
	int64_t max_frames_ahead;
	int64_t timeline_max_frame;
	bool should_pause_cache;
	bool should_break;

	/// Constructor
	VideoCacheThread();
	/// Destructor
	~VideoCacheThread();

    /// Get the size in bytes of a frame (rough estimate)
    int64_t getBytes(int width, int height, int sample_rate, int channels, float fps);

    /// Get Speed (The speed and direction to playback a reader (1=normal, 2=fast, 3=faster, -1=rewind, etc...)
    int getSpeed() const { return speed; }

	/// Play the video
	void Play();

    /// Seek the reader to a particular frame number
    void Seek(int64_t new_position);

	/// Seek the reader to a particular frame number and optionally start the pre-roll
	void Seek(int64_t new_position, bool start_preroll);

	/// Set Speed (The speed and direction to playback a reader (1=normal, 2=fast, 3=faster, -1=rewind, etc...)
	void setSpeed(int new_speed);

	/// Stop the audio playback
	void Stop();

	/// Start the thread
	void run();

	/// Set the current thread's reader
	void Reader(ReaderBase *new_reader) { reader=new_reader; Play(); };

	/// Parent class of VideoCacheThread
	friend class PlayerPrivate;
	friend class QtPlayer;

    public:
        /// Is cache ready for video/audio playback
        bool isReady();
    };
}

#endif // OPENSHOT_VIDEO_CACHE_THREAD_H
