/**
 * @file
 * @brief Header file for VideoCacheThread class
 * @author Jonathan Thomas <jonathan@openshot.org>
 *
 * @ref License
 */

// Copyright (c) 2008-2025 OpenShot Studios, LLC
//
// SPDX-License-Identifier: LGPL-3.0-or-later

#ifndef OPENSHOT_VIDEO_CACHE_THREAD_H
#define OPENSHOT_VIDEO_CACHE_THREAD_H

#include "ReaderBase.h"

#include <AppConfig.h>
#include <juce_audio_basics/juce_audio_basics.h>
#include <memory>

namespace openshot
{
    using juce::Thread;

    /**
     * @brief Handles prefetching and caching of video/audio frames for smooth playback.
     *
     * This thread continuously maintains a “window” of cached frames in the current playback
     * direction (forward or backward). When paused, it continues to fill that same window;
     * when seeking, it resets to cache around the new position.
     */
    class VideoCacheThread : public Thread
    {
    public:
        /// Constructor: initializes member variables and assumes forward direction on first launch.
        VideoCacheThread();
        ~VideoCacheThread() override;

        /// @return True if at least min_frames_ahead frames have been cached.
        bool isReady();

        /// Play method is unimplemented
        void Play() {};

        /// Stop method is unimplemented
        void Stop() {};

        /**
         * @brief Set playback speed/direction. Positive = forward, negative = rewind, zero = pause.
         * @param new_speed
         *
         * If new_speed != 0, last_speed and last_dir are updated.
         * If new_speed == 0, last_dir is left unchanged so that pausing does not flip direction.
         */
        void setSpeed(int new_speed);

        /// @return The current speed (1=normal, 2=fast, –1=rewind, etc.)
        int getSpeed() const { return speed; }

        /// Seek to a specific frame (no preroll).
        void Seek(int64_t new_position);

        /**
         * @brief Seek to a specific frame and optionally start a preroll (cache reset).
         * @param new_position Frame index to jump to.
         * @param start_preroll If true, forces cache to rebuild around new_position.
         */
        void Seek(int64_t new_position, bool start_preroll);

        /// Start the cache thread at high priority. Returns true if it’s actually running.
        bool StartThread();

        /// Stop the cache thread (wait up to timeoutMs ms). Returns true if it stopped.
        bool StopThread(int timeoutMs = 0);

        /**
         * @brief Attach a ReaderBase (e.g. Timeline, FFmpegReader) and begin caching.
         * @param new_reader
         */
        void Reader(ReaderBase* new_reader) { reader = new_reader; Play(); }

    protected:
        /// Thread entry point: loops until threadShouldExit() is true.
        void run() override;

        /**
         * @brief Estimate memory usage for a single frame (video + audio).
         * @param width       Frame width (pixels)
         * @param height      Frame height (pixels)
         * @param sample_rate Audio sample rate (e.g. 48000)
         * @param channels    Number of audio channels
         * @param fps         Frames per second
         * @return Approximate size in bytes for one frame
         */
        int64_t getBytes(int width, int height, int sample_rate, int channels, float fps);

        //---------- Helper functions, broken out for clarity & unit testing ----------

        /// @return Effective playback direction (+1 forward, –1 backward), preserving last_dir if speed == 0.
        int computeDirection() const;

        /**
         * @brief If userSeeked is true, reset last_cached_index just behind the playhead.
         * @param playhead Current requested_display_frame
         * @param dir      Effective direction (±1)
         */
        void handleUserSeek(int64_t playhead, int dir);

        /**
         * @brief When paused and playhead is outside current cache, clear all frames.
         * @param playhead Current requested_display_frame
         * @param paused   True if speed == 0
         * @param cache    Pointer to CacheBase
         * @return True if ClearAllCache() was invoked.
         */
        bool clearCacheIfPaused(int64_t playhead, bool paused, CacheBase* cache);

        /**
         * @brief Compute the “window” of frames to cache around playhead.
         * @param playhead     Current requested_display_frame
         * @param dir          Effective direction (±1)
         * @param ahead_count  Number of frames ahead/back to cache
         * @param timeline_end Last valid frame index
         * @param[out] window_begin Lower bound (inclusive) of caching window
         * @param[out] window_end   Upper bound (inclusive) of caching window
         *
         * If dir > 0: window = [playhead ... playhead + ahead_count]
         * If dir < 0: window = [playhead – ahead_count ... playhead]
         * Always clamps to [1 ... timeline_end].
         */
        void computeWindowBounds(int64_t playhead,
                                 int dir,
                                 int64_t ahead_count,
                                 int64_t timeline_end,
                                 int64_t& window_begin,
                                 int64_t& window_end) const;

        /**
         * @brief Prefetch all missing frames in [window_begin ... window_end] or [window_end ... window_begin].
         * @param cache          Pointer to CacheBase
         * @param window_begin   Inclusive lower bound of the window
         * @param window_end     Inclusive upper bound of the window
         * @param dir            Effective direction (±1)
         * @param reader         Pointer to ReaderBase to call GetFrame()
         * @return True if the window was already full (no new frames added)
         *
         * Internally, this method iterates from last_cached_index + dir toward window_end (or window_begin)
         * and calls GetFrame()/Add() for each missing frame until hitting the window boundary or an OOB.
         * It also breaks early if threadShouldExit() or userSeeked becomes true.
         */
        bool prefetchWindow(CacheBase* cache,
                            int64_t window_begin,
                            int64_t window_end,
                            int dir,
                            ReaderBase* reader);

        //---------- Internal state ----------

        std::shared_ptr<Frame> last_cached_frame; ///< Last frame pointer added to cache.

        int speed;            ///< Current playback speed (0=paused, >0 forward, <0 backward).
        int last_speed;       ///< Last non-zero speed (for tracking).
        int last_dir;         ///< Last direction sign (+1 forward, –1 backward).
        bool userSeeked;      ///< True if Seek(..., true) was called (forces a cache reset).

        int64_t requested_display_frame; ///< Frame index the user requested.
        int64_t current_display_frame;   ///< Currently displayed frame (unused here, reserved).
        int64_t cached_frame_count;      ///< Count of frames currently added to cache.

        int64_t min_frames_ahead;        ///< Minimum number of frames considered “ready” (pre-roll).
        int64_t timeline_max_frame;      ///< Highest valid frame index in the timeline.

        ReaderBase* reader;              ///< The source reader (e.g., Timeline, FFmpegReader).
        bool force_directional_cache;    ///< (Reserved for future use).

        int64_t last_cached_index;       ///< Index of the most recently cached frame.
    };

} // namespace openshot

#endif // OPENSHOT_VIDEO_CACHE_THREAD_H
