/**
 * @file
 * @brief Source file for VideoCacheThread class
 * @author Jonathan Thomas <jonathan@openshot.org>
 *
 * @ref License
 */

// Copyright (c) 2008-2025 OpenShot Studios, LLC
//
// SPDX-License-Identifier: LGPL-3.0-or-later

#include "VideoCacheThread.h"
#include "CacheBase.h"
#include "Exceptions.h"
#include "Frame.h"
#include "Settings.h"
#include "Timeline.h"
#include <thread>
#include <chrono>
#include <algorithm>

namespace openshot
{
    // Constructor
    VideoCacheThread::VideoCacheThread()
        : Thread("video-cache")
        , speed(0)
        , last_speed(1)
        , last_dir(1)                   // assume forward (+1) on first launch
        , userSeeked(false)
        , requested_display_frame(1)
        , current_display_frame(1)
        , cached_frame_count(0)
        , min_frames_ahead(4)
        , timeline_max_frame(0)
        , reader(nullptr)
        , force_directional_cache(false)
        , last_cached_index(0)
    {
    }

    // Destructor
    VideoCacheThread::~VideoCacheThread()
    {
    }

    // Is cache ready for playback (pre-roll)
    bool VideoCacheThread::isReady()
    {
        return (cached_frame_count > min_frames_ahead);
    }

    void VideoCacheThread::setSpeed(int new_speed)
    {
        // Only update last_speed and last_dir when new_speed != 0
        if (new_speed != 0) {
            last_speed = new_speed;
            last_dir   = (new_speed > 0 ? 1 : -1);
        }
        speed = new_speed;
    }

    // Get the size in bytes of a frame (rough estimate)
    int64_t VideoCacheThread::getBytes(int width,
                                       int height,
                                       int sample_rate,
                                       int channels,
                                       float fps)
    {
        // RGBA video frame
        int64_t bytes = static_cast<int64_t>(width) * height * sizeof(char) * 4;
        // Approximate audio: (sample_rate * channels)/fps samples per frame
        bytes += ((sample_rate * channels) / fps) * sizeof(float);
        return bytes;
    }

    /// Start the cache thread at high priority, and return true if it’s actually running.
    bool VideoCacheThread::StartThread()
    {
        // JUCE’s startThread() returns void, so we launch it and then check if
        // the thread actually started:
        startThread(Priority::high);
        return isThreadRunning();
    }

    /// Stop the cache thread, waiting up to timeoutMs ms. Returns true if it actually stopped.
    bool VideoCacheThread::StopThread(int timeoutMs)
    {
        stopThread(timeoutMs);
        return !isThreadRunning();
    }

    void VideoCacheThread::Seek(int64_t new_position, bool start_preroll)
    {
        if (start_preroll) {
            userSeeked = true;

            if (!reader->GetCache()->Contains(new_position))
            {
                // If user initiated seek, and current frame not found (
                Timeline* timeline = static_cast<Timeline*>(reader);
                timeline->ClearAllCache();
            }
        }
        requested_display_frame = new_position;
    }

    void VideoCacheThread::Seek(int64_t new_position)
    {
        Seek(new_position, false);
    }

    int VideoCacheThread::computeDirection() const
    {
        // If speed ≠ 0, use its sign; if speed==0, keep last_dir
        return (speed != 0 ? (speed > 0 ? 1 : -1) : last_dir);
    }

    void VideoCacheThread::handleUserSeek(int64_t playhead, int dir)
    {
        // Place last_cached_index just “behind” playhead in the given dir
        last_cached_index = playhead - dir;
    }

    bool VideoCacheThread::clearCacheIfPaused(int64_t playhead,
                                              bool paused,
                                              CacheBase* cache)
    {
        if (paused && !cache->Contains(playhead)) {
            // If paused and playhead not in cache, clear everything
            Timeline* timeline = static_cast<Timeline*>(reader);
            timeline->ClearAllCache();
            return true;
        }
        return false;
    }

    void VideoCacheThread::computeWindowBounds(int64_t playhead,
                                               int dir,
                                               int64_t ahead_count,
                                               int64_t timeline_end,
                                               int64_t& window_begin,
                                               int64_t& window_end) const
    {
        if (dir > 0) {
            // Forward window: [playhead ... playhead + ahead_count]
            window_begin = playhead;
            window_end   = playhead + ahead_count;
        }
        else {
            // Backward window: [playhead - ahead_count ... playhead]
            window_begin = playhead - ahead_count;
            window_end   = playhead;
        }
        // Clamp to [1 ... timeline_end]
        window_begin = std::max<int64_t>(window_begin, 1);
        window_end   = std::min<int64_t>(window_end, timeline_end);
    }

    bool VideoCacheThread::prefetchWindow(CacheBase* cache,
                                          int64_t window_begin,
                                          int64_t window_end,
                                          int dir,
                                          ReaderBase* reader)
    {
        bool window_full = true;
        int64_t next_frame = last_cached_index + dir;

        // Advance from last_cached_index toward window boundary
        while ((dir > 0 && next_frame <= window_end) ||
               (dir < 0 && next_frame >= window_begin))
        {
            if (threadShouldExit()) {
                break;
            }
            // If a Seek was requested mid-caching, bail out immediately
            if (userSeeked) {
                break;
            }

            if (!cache->Contains(next_frame)) {
                // Frame missing, fetch and add
                try {
                    auto framePtr = reader->GetFrame(next_frame);
                    cache->Add(framePtr);
                    ++cached_frame_count;
                }
                catch (const OutOfBoundsFrame&) {
                    break;
                }
                window_full = false;
            }
            else {
                cache->Touch(next_frame);
            }

            last_cached_index = next_frame;
            next_frame       += dir;
        }

        return window_full;
    }

    void VideoCacheThread::run()
    {
        using micro_sec        = std::chrono::microseconds;
        using double_micro_sec = std::chrono::duration<double, micro_sec::period>;

        while (!threadShouldExit()) {
            Settings* settings = Settings::Instance();
            CacheBase* cache   = reader ? reader->GetCache() : nullptr;

            // If caching disabled or no reader, sleep briefly
            if (!settings->ENABLE_PLAYBACK_CACHING || !cache) {
                std::this_thread::sleep_for(double_micro_sec(50000));
                continue;
            }

            // init local vars
            min_frames_ahead = settings->VIDEO_CACHE_MIN_PREROLL_FRAMES;

            Timeline* timeline    = static_cast<Timeline*>(reader);
            int64_t  timeline_end = timeline->GetMaxFrame();
            int64_t  playhead     = requested_display_frame;
            bool     paused       = (speed == 0);

            // Compute effective direction (±1)
            int dir = computeDirection();
            if (speed != 0) {
                last_dir = dir;
            }

            // Compute bytes_per_frame, max_bytes, and capacity once
            int64_t bytes_per_frame = getBytes(
                (timeline->preview_width ? timeline->preview_width : reader->info.width),
                (timeline->preview_height ? timeline->preview_height : reader->info.height),
                reader->info.sample_rate,
                reader->info.channels,
                reader->info.fps.ToFloat()
            );
            int64_t max_bytes = cache->GetMaxBytes();
            int64_t capacity  = 0;
            if (max_bytes > 0 && bytes_per_frame > 0) {
                capacity = max_bytes / bytes_per_frame;
                if (capacity > settings->VIDEO_CACHE_MAX_FRAMES) {
                    capacity = settings->VIDEO_CACHE_MAX_FRAMES;
                }
            }

            // Handle a user-initiated seek
            if (userSeeked) {
                handleUserSeek(playhead, dir);
                userSeeked = false;
            }
            else if (!paused && capacity >= 1) {
                // In playback mode, check if last_cached_index drifted outside the new window
                int64_t base_ahead = static_cast<int64_t>(capacity * settings->VIDEO_CACHE_PERCENT_AHEAD);

                int64_t window_begin, window_end;
                computeWindowBounds(
                    playhead,
                    dir,
                    base_ahead,
                    timeline_end,
                    window_begin,
                    window_end
                );

                bool outside_window =
                    (dir > 0 && last_cached_index > window_end) ||
                    (dir < 0 && last_cached_index < window_begin);
                if (outside_window) {
                    handleUserSeek(playhead, dir);
                }
            }

            // If capacity is insufficient, sleep and retry
            if (capacity < 1) {
                std::this_thread::sleep_for(double_micro_sec(50000));
                continue;
            }
            int64_t ahead_count = static_cast<int64_t>(capacity *
                                           settings->VIDEO_CACHE_PERCENT_AHEAD);

            // If paused and playhead is no longer in cache, clear everything
            bool did_clear = clearCacheIfPaused(playhead, paused, cache);
            if (did_clear) {
                handleUserSeek(playhead, dir);
            }

            // Compute the current caching window
            int64_t window_begin, window_end;
            computeWindowBounds(playhead,
                                dir,
                                ahead_count,
                                timeline_end,
                                window_begin,
                                window_end);

            // Attempt to fill any missing frames in that window
            bool window_full = prefetchWindow(cache, window_begin, window_end, dir, reader);

            // If paused and window was already full, keep playhead fresh
            if (paused && window_full) {
                cache->Touch(playhead);
            }

            // Sleep a short fraction of a frame interval
            int64_t sleep_us = static_cast<int64_t>(
                1000000.0 / reader->info.fps.ToFloat() / 4.0
            );
            std::this_thread::sleep_for(double_micro_sec(sleep_us));
        }
    }

} // namespace openshot
