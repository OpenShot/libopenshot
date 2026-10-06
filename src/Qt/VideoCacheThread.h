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
#include "AdaptivePreroll.h"

#include <AppConfig.h>
#include <juce_audio_basics/juce_audio_basics.h>
#include <atomic>
#include <cstdint>
#include <mutex>
#include <memory>

namespace openshot
{
    class Settings;
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

        /// Shared startup gate; bounded fallback resumes established video decoding.
        bool isReady();
        /// Callback miss holds the shared gate until the player resynchronizes audio.
        bool UsesCachedAudio() const { return cache_audio_only.load(); }
        void NotifyAudioCacheMiss() { audio_cache_miss.store(true); }
        void AcknowledgeAudioCacheMiss() { audio_cache_miss.store(false); }

        /// Play method is unimplemented
        void Play() { notify(); };

        /// Stop method is unimplemented
        void Stop();

        /**
         * @brief Set playback speed/direction. Positive = forward, negative = rewind, zero = pause.
         * @param new_speed
         *
         * If new_speed != 0, last_speed and last_dir are updated.
         * If new_speed == 0, last_dir is left unchanged so that pausing does not flip direction.
         */
        void setSpeed(int new_speed);

        /// @return The current speed (1=normal, 2=fast, –1=rewind, etc.)
        int getSpeed() const { return speed.load(); }

        /// Backward-compatible alias for playback position updates (no seek side effects).
        void Seek(int64_t new_position);

        /**
         * @brief Seek to a specific frame and optionally start a preroll (cache reset).
         * @param new_position Frame index to jump to.
         * @param start_preroll If true, forces cache to rebuild around new_position.
         */
        void Seek(int64_t new_position, bool start_preroll);

        /// Update playback position without triggering seek behavior or cache invalidation.
        void NotifyPlaybackPosition(int64_t new_position);

        /// Start the cache thread at high priority. Returns true if it’s actually running.
        bool StartThread();

        /// Stop the cache thread (wait up to timeoutMs ms). Returns true if it stopped.
        bool StopThread(int timeoutMs = 0);

        /// Optional request timings in microseconds (zero until observed).
        /// No per-frame logging; clocks are sampled only when enabled.
        void EnableRequestDiagnostics(bool enabled) { diagnostics_enabled.store(enabled); }
        int64_t CancellationAcknowledgementUs() const { return acknowledgement_us.load(); }
        int64_t ObsoleteCompletionUs() const { return obsolete_completion_us.load(); }
        int64_t FirstCurrentFrameUs() const { return first_current_frame_us.load(); }

        /**
         * @brief Attach a ReaderBase (e.g. Timeline, FFmpegReader) and begin caching.
         * @param new_reader
         */
        void Reader(ReaderBase* new_reader);

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
         * @brief Reset last_cached_index to start caching with a directional preroll offset.
         * @param playhead        Current requested_display_frame
         * @param dir             Effective direction (±1)
         * @param timeline_end    Last valid frame index
         * @param preroll_frames  Number of frames to offset the cache start
         */
        void handleUserSeekWithPreroll(int64_t playhead,
                                       int dir,
                                       int64_t timeline_end,
                                       int64_t preroll_frames);

        /// @brief Compute preroll frame count from settings.
        int64_t computePrerollFrames(const Settings* settings) const;

        /// @brief Resolve timeline end frame from reader/timeline metadata.
        int64_t resolveTimelineEnd() const;

        /// @brief Clamp frame index to [1, timeline_end] when timeline_end is valid.
        int64_t clampToTimelineRange(int64_t frame, int64_t timeline_end) const;

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
                            ReaderBase* reader,
                            int64_t max_frames_to_fetch = -1,
                            uint64_t generation = UINT64_MAX);

        /// Worker-only cache probing; never runs in the audio callback.
        void updateReadiness(CacheBase* cache, int64_t playhead, int dir);
        void resetReadiness(); // Requires seek_state_mutex.

        AdaptivePreroll preroll_policy;
        int64_t contiguous_ahead = 0;
        int64_t readiness_playhead = 1;
        int readiness_direction = 1;
        std::atomic<bool> audio_cache_miss{false};
        std::atomic<bool> cache_audio_only{false};
        int64_t observed_frame_bytes = 0;
        int64_t readiness_capacity = 0;
        int64_t readiness_physical_capacity = 0;
        int64_t readiness_timeline_end = 0;
        int64_t readiness_bytes_per_frame = 0;
        uint64_t readiness_epoch = 0;
        int64_t readiness_minimum_step = 0;

        //---------- Internal state ----------

        std::shared_ptr<Frame> last_cached_frame; ///< Last frame pointer added to cache.

        std::atomic<int> speed;            ///< Current playback speed (0=paused, >0 forward, <0 backward).
        std::atomic<int> last_speed;       ///< Last non-zero speed (for tracking).
        std::atomic<int> last_dir;         ///< Last direction sign (+1 forward, –1 backward).
        std::atomic<bool> userSeeked;      ///< True if Seek(..., true) was called (forces a cache reset).
        std::atomic<bool> preroll_on_next_fill; ///< True if next cache rebuild should include preroll offset.
        std::atomic<bool> clear_cache_on_next_fill; ///< True if next cache loop should clear existing cache ranges.
        std::atomic<bool> scrub_active;    ///< True while user is dragging/scrubbing the playhead.

        std::atomic<int64_t> requested_display_frame; ///< Frame index the user requested.
        int64_t current_display_frame;   ///< Currently displayed frame (unused here, reserved).
        std::atomic<int64_t> cached_frame_count;      ///< Estimated count of frames currently stored in cache.

        std::atomic<int64_t> min_frames_ahead;        ///< Minimum number of frames considered “ready” (pre-roll).
        int64_t timeline_max_frame;      ///< Highest valid frame index in the timeline.

        ReaderBase* reader;              ///< The source reader (e.g., Timeline, FFmpegReader).
        bool force_directional_cache;    ///< (Reserved for future use).
        uint64_t seen_timeline_cache_epoch; ///< Last observed Timeline cache invalidation epoch.
        bool timeline_cache_epoch_initialized; ///< True once an initial epoch snapshot has been taken.

        std::atomic<int64_t> last_cached_index;       ///< Index of the most recently cached frame.
        std::atomic<uint64_t> processed_generation{UINT64_MAX}; ///< Last request whose scheduling baseline was consumed.
        std::atomic<uint64_t> request_generation{0}; ///< Invalidates obsolete fills, including seeks retaining cache.
        std::atomic<bool> diagnostics_enabled{false};
        std::atomic<int64_t> request_time_us{0};
        std::atomic<int64_t> acknowledgement_us{0};
        std::atomic<int64_t> obsolete_completion_us{0};
        std::atomic<int64_t> first_current_frame_us{0};
        void acknowledgeRequest(int64_t started_us);
        static int64_t monotonicUs();
        mutable std::mutex lifecycle_mutex; ///< Start/stop/reader drain; never acquired by worker.
        mutable std::mutex seek_state_mutex;          ///< Protects coherent seek state updates/consumption.
    };

} // namespace openshot

#endif // OPENSHOT_VIDEO_CACHE_THREAD_H
