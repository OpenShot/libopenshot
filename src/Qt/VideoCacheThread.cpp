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
        , preroll_on_next_fill(false)
        , clear_cache_on_next_fill(false)
        , scrub_active(false)
        , requested_display_frame(1)
        , current_display_frame(1)
        , cached_frame_count(0)
        , min_frames_ahead(4)
        , timeline_max_frame(0)
        , reader(nullptr)
        , force_directional_cache(false)
        , last_cached_index(0)
        , seen_timeline_cache_epoch(0)
        , timeline_cache_epoch_initialized(false)
    {
    }

    // Destructor
    VideoCacheThread::~VideoCacheThread()
    {
        // Raw readers remain caller-owned. Do not destroy state while decoding.
        StopThread(-1);
    }

    // Is cache ready for playback (pre-roll)
    bool VideoCacheThread::isReady()
    {
        std::lock_guard<std::mutex> guard(seek_state_mutex);
        if (!reader) {
            return false;
        }

        if (audio_cache_miss.load()) return false;
        const auto* settings = Settings::Instance();
        auto* timeline = dynamic_cast<Timeline*>(reader);
        if (!settings->ENABLE_PLAYBACK_CACHING || !timeline || !reader->GetCache()
            || settings->VIDEO_CACHE_MIN_PREROLL_FRAMES < 0)
            return true;
        if (timeline->CacheEpoch() != readiness_epoch) {
            resetReadiness();
            readiness_epoch = timeline->CacheEpoch();
        }
        if (readiness_physical_capacity == 0 && processed_generation.load() == request_generation.load())
            return true;
        const int64_t now = monotonicUs();
        const bool current_request = processed_generation.load() == request_generation.load()
            && !timeline->CacheRefreshPending();
        const int dir = computeDirection();
        const int64_t playhead = clampToTimelineRange(requested_display_frame.load(), readiness_timeline_end);
        const int64_t shifted = (playhead - readiness_playhead) * dir;
        const int64_t contiguous = readiness_direction == dir && shifted >= 0
            ? std::max<int64_t>(0, contiguous_ahead - shifted) : 0;
        CacheBase* cache = reader->GetCache();
        // Only two bounded cache lookups in the callback; the worker probes the
        // larger startup range. These also reject holes introduced by eviction.
        const bool playable = cache->Contains(playhead)
            && (readiness_minimum_step == 0 || cache->Contains(playhead + readiness_minimum_step * dir));
        const int64_t checked_ahead = preroll_policy.Released() && playable
            ? readiness_minimum_step : contiguous;
        return preroll_policy.Ready(now, current_request ? checked_ahead : 0,
            current_request && playable, min_frames_ahead.load(), readiness_minimum_step);
    }

    void VideoCacheThread::resetReadiness()
    {
        preroll_policy.Reset(monotonicUs());
        contiguous_ahead = 0;
    }

    void VideoCacheThread::updateReadiness(CacheBase* cache, int64_t playhead, int dir)
    {
        // Called by the worker under seek_state_mutex, never while decoding.
        readiness_playhead = playhead;
        readiness_direction = dir;
        contiguous_ahead = 0;
        const int64_t required = std::max(min_frames_ahead.load(), readiness_minimum_step);
        while (contiguous_ahead < required && cache->Contains(playhead + (contiguous_ahead + 1) * dir))
            ++contiguous_ahead;
    }

    void VideoCacheThread::setSpeed(int new_speed)
    {
        std::lock_guard<std::mutex> guard(seek_state_mutex);
        const int old_direction = computeDirection();
        if (new_speed != speed.load()) {
            request_generation.fetch_add(1);
            resetReadiness();
        }
        // Only update last_speed and last_dir when new_speed != 0
        if (new_speed != 0) {
            last_speed.store(new_speed);
            last_dir.store(new_speed > 0 ? 1 : -1);
            // Leaving paused/scrub context: resume normal cache behavior.
            scrub_active.store(false);
        }
        speed.store(new_speed);
        if (computeDirection() != old_direction) {
            userSeeked.store(true);
            last_cached_index.store(requested_display_frame.load() - computeDirection());
        }
        notify();
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
        if (fps > 0) bytes += (static_cast<double>(sample_rate) * channels / fps) * sizeof(float);
        return bytes;
    }

    int64_t VideoCacheThread::monotonicUs()
    {
        return std::chrono::duration_cast<std::chrono::microseconds>(
            std::chrono::steady_clock::now().time_since_epoch()).count();
    }

    void VideoCacheThread::acknowledgeRequest(int64_t started_us)
    {
        request_generation.fetch_add(1);
        if (started_us) {
            request_time_us.store(started_us);
            first_current_frame_us.store(0);
            obsolete_completion_us.store(0);
            acknowledgement_us.store(monotonicUs() - started_us);
        }
        notify();
    }

    bool VideoCacheThread::StartThread()
    {
        std::lock_guard<std::mutex> guard(lifecycle_mutex);
        // A timed stop may leave a decoder draining. Never start another worker.
        if (isThreadRunning())
            return !threadShouldExit();
        startThread(Priority::high);
        return isThreadRunning();
    }

    void VideoCacheThread::Stop()
    {
        const int64_t started = diagnostics_enabled.load() ? monotonicUs() : 0;
        std::lock_guard<std::mutex> guard(seek_state_mutex);
        acknowledgeRequest(started);
        signalThreadShouldExit();
        notify();
    }

    bool VideoCacheThread::StopThread(int timeoutMs)
    {
        std::lock_guard<std::mutex> guard(lifecycle_mutex);
        Stop();
        // JUCE stopThread(timeout) forcibly kills on timeout. A timeout here
        // reports unfinished work; destruction/replacement explicitly drains.
        return waitForThreadToExit(timeoutMs);
    }

    void VideoCacheThread::Reader(ReaderBase* new_reader)
    {
        std::lock_guard<std::mutex> lifecycle_guard(lifecycle_mutex);
        const bool restart = isThreadRunning();
        if (restart) {
            Stop();
            waitForThreadToExit(-1);
        }
        {
            std::lock_guard<std::mutex> guard(seek_state_mutex);
            request_generation.fetch_add(1);
            reader = new_reader;
            cache_audio_only.store(false);
            audio_cache_miss.store(false);
            resetReadiness();
            seen_timeline_cache_epoch = 0;
            timeline_cache_epoch_initialized = false;
            userSeeked.store(false);
            last_cached_index.store(requested_display_frame.load() - computeDirection());
        }
        if (restart)
            startThread(Priority::high);
    }

    void VideoCacheThread::Seek(int64_t new_position, bool start_preroll)
    {
        const int64_t started = diagnostics_enabled.load() ? monotonicUs() : 0;
        std::lock_guard<std::mutex> guard(seek_state_mutex);
        const int64_t timeline_end = resolveTimelineEnd();
        const int64_t clamped_new_position = clampToTimelineRange(new_position, timeline_end);
        const int64_t current_requested = requested_display_frame.load();

        bool should_mark_seek = false;
        bool should_preroll = false;
        int64_t new_cached_count = cached_frame_count.load();
        bool entering_scrub = false;
        bool leaving_scrub = false;
        bool cache_contains = false;
        bool should_clear_cache = false;
        CacheBase* cache = reader ? reader->GetCache() : nullptr;
        const bool same_frame_refresh = (new_position == current_requested);
        if (cache) {
            cache_contains = cache->Contains(clamped_new_position);
        }

        if (start_preroll) {
            if (same_frame_refresh) {
                const bool is_paused = (speed.load() == 0);
                if (is_paused) {
                    const bool was_scrubbing = scrub_active.load();
                    if (was_scrubbing && cache && cache_contains) {
                        // Preserve in-range cache for paused scrub preview -> same-frame commit.
                        should_mark_seek = false;
                        should_preroll = false;
                        should_clear_cache = false;
                        new_cached_count = cache->Count();
                    } else {
                        // Paused same-frame edit refresh: force full cache refresh.
                        if (Timeline* timeline = dynamic_cast<Timeline*>(reader)) {
                            timeline->RequestClearAllCache();
                        }
                        new_cached_count = 0;
                        should_mark_seek = true;
                        should_preroll = true;
                        should_clear_cache = false;
                    }
                } else {
                    // Same-frame refresh during playback should stay lightweight.
                    should_mark_seek = false;
                    should_preroll = false;
                    should_clear_cache = false;
                    if (cache && cache_contains) {
                        cache->Remove(clamped_new_position);
                    }
                    if (cache) {
                        new_cached_count = cache->Count();
                    }
                }
            } else {
                if (cache && !cache_contains) {
                    should_mark_seek = true;
                    // Uncached commit seek: defer cache clear to cache thread loop.
                    new_cached_count = 0;
                    should_preroll = true;
                    should_clear_cache = true;
                }
                else if (cache)
                {
                    // In-range commit seek preserves cache window/baseline.
                    should_mark_seek = false;
                    should_preroll = false;
                    should_clear_cache = false;
                    new_cached_count = cache->Count();
                } else {
                    // No cache object to query: use normal seek behavior.
                    should_mark_seek = true;
                }
            }
            leaving_scrub = true;
        }
        else {
            // Non-preroll seeks cover paused scrubbing and live playback refresh.
            const bool is_paused = (speed.load() == 0);
            if (is_paused && same_frame_refresh) {
                // Same-frame paused refresh updates only that frame.
                should_mark_seek = false;
                should_preroll = false;
                should_clear_cache = false;
                if (cache && cache_contains) {
                    cache->Remove(clamped_new_position);
                }
                if (cache) {
                    new_cached_count = cache->Count();
                }
                leaving_scrub = true;
            }
            else if (is_paused) {
                if (cache && !cache_contains) {
                    should_mark_seek = true;
                    new_cached_count = 0;
                    should_clear_cache = true;
                }
                else if (cache) {
                    // In-range paused seek preserves cache continuity.
                    should_mark_seek = false;
                    new_cached_count = cache->Count();
                } else {
                    should_mark_seek = true;
                }
                entering_scrub = true;
            } else {
                // During playback, keep seek/scrub side effects minimal.
                should_mark_seek = false;
                should_preroll = false;
                should_clear_cache = false;
                if (cache) {
                    new_cached_count = cache->Count();
                }
                leaving_scrub = true;
            }
        }

        {
            // Reset readiness baseline only when rebuilding cache.
            const int dir = computeDirection();
            if (should_mark_seek || should_preroll || should_clear_cache) {
                last_cached_index.store(clamped_new_position - dir);
            }
            requested_display_frame.store(new_position);
            cached_frame_count.store(new_cached_count);
            preroll_on_next_fill.store(should_preroll);
            // Clear behavior follows the latest seek intent.
            clear_cache_on_next_fill.store(should_clear_cache);
            userSeeked.store(should_mark_seek);
            if (entering_scrub) {
                scrub_active.store(true);
            }
            if (leaving_scrub) {
                scrub_active.store(false);
            }
        }
        resetReadiness();
        acknowledgeRequest(started);
    }

    void VideoCacheThread::Seek(int64_t new_position)
    {
        NotifyPlaybackPosition(new_position);
    }

    void VideoCacheThread::NotifyPlaybackPosition(int64_t new_position)
    {
        std::lock_guard<std::mutex> guard(seek_state_mutex);
        if (new_position <= 0) {
            return;
        }
        if (scrub_active.load()) {
            return;
        }

        int64_t new_cached_count = cached_frame_count.load();
        if (CacheBase* cache = reader ? reader->GetCache() : nullptr) {
            new_cached_count = cache->Count();
        }
        {
            requested_display_frame.store(new_position);
            cached_frame_count.store(new_cached_count);
        }
    }

    int VideoCacheThread::computeDirection() const
    {
        // If speed ≠ 0, use its sign; if speed==0, keep last_dir
        const int current_speed = speed.load();
        if (current_speed != 0) {
            return (current_speed > 0 ? 1 : -1);
        }
        return last_dir.load();
    }

    void VideoCacheThread::handleUserSeek(int64_t playhead, int dir)
    {
        // Place last_cached_index just “behind” playhead in the given dir
        last_cached_index.store(playhead - dir);
    }

    void VideoCacheThread::handleUserSeekWithPreroll(int64_t playhead,
                                                     int dir,
                                                     int64_t timeline_end,
                                                     int64_t preroll_frames)
    {
        int64_t preroll_start = playhead;
        if (preroll_frames > 0) {
            if (dir > 0) {
                preroll_start = std::max<int64_t>(1, playhead - preroll_frames);
            }
            else {
                preroll_start = std::min<int64_t>(timeline_end, playhead + preroll_frames);
            }
        }
        last_cached_index.store(preroll_start - dir);
    }

    int64_t VideoCacheThread::computePrerollFrames(const Settings* settings) const
    {
        if (!settings) {
            return 0;
        }
        int64_t min_frames = settings->VIDEO_CACHE_MIN_PREROLL_FRAMES;
        int64_t max_frames = settings->VIDEO_CACHE_MAX_PREROLL_FRAMES;
        if (min_frames < 0) {
            return 0;
        }
        if (max_frames > 0 && min_frames > max_frames) {
            min_frames = max_frames;
        }
        return min_frames;
    }

    int64_t VideoCacheThread::resolveTimelineEnd() const
    {
        if (!reader) {
            return 0;
        }
        int64_t timeline_end = reader->info.video_length;
        if (auto* timeline = dynamic_cast<Timeline*>(reader)) {
            const int64_t timeline_max = timeline->GetMaxFrame();
            if (timeline_max > 0) {
                timeline_end = timeline_max;
            }
        }
        return timeline_end;
    }

    int64_t VideoCacheThread::clampToTimelineRange(int64_t frame, int64_t timeline_end) const
    {
        if (timeline_end < 1) {
            return frame;
        }
        return std::clamp<int64_t>(frame, 1, timeline_end);
    }

    bool VideoCacheThread::clearCacheIfPaused(int64_t playhead,
                                              bool paused,
                                              CacheBase* cache)
    {
        const int64_t timeline_end = resolveTimelineEnd();
        int64_t cache_playhead = playhead;
        if (reader) {
            cache_playhead = clampToTimelineRange(playhead, timeline_end);
        }
        if (paused && !cache->Contains(cache_playhead)) {
            // If paused and playhead not in cache, clear everything
            if (Timeline* timeline = dynamic_cast<Timeline*>(reader)) {
                timeline->ClearAllCache();
            }
            cached_frame_count.store(0);
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
                                          ReaderBase* reader,
                                          int64_t max_frames_to_fetch,
                                          uint64_t generation)
    {
        if (generation == UINT64_MAX)
            generation = request_generation.load();
        auto* timeline = dynamic_cast<Timeline*>(reader);
        const uint64_t epoch = timeline ? timeline->CacheEpoch() : 0;
        bool window_full = true;
        int64_t next_frame = last_cached_index.load() + dir;
        int64_t fetched_this_pass = 0;

        // Advance from last_cached_index toward window boundary
        while ((dir > 0 && next_frame <= window_end) ||
               (dir < 0 && next_frame >= window_begin))
        {
            if (threadShouldExit()) {
                break;
            }
            // If a Seek was requested mid-caching, bail out immediately
            if (userSeeked.load() || request_generation.load() != generation
                || (timeline && timeline->CacheEpoch() != epoch)) {
                break;
            }

            if ((timeline && timeline->CacheRefreshPending()) || !cache->Contains(next_frame)) {
                // Frame missing, fetch and add
                try {
                    const int64_t decode_started = monotonicUs();
                    auto framePtr = reader->GetFrame(next_frame);
                    const int64_t decode_us = monotonicUs() - decode_started;
                    // The reader may have cached a valid reusable frame itself.
                    // Timeline edits own invalidation under their decoder lock;
                    // do not re-add its result after that lock has been released.
                    std::lock_guard<std::mutex> guard(seek_state_mutex);
                    if (threadShouldExit() || request_generation.load() != generation
                        || (timeline && timeline->CacheEpoch() != epoch)) {
                        if (diagnostics_enabled.load() && request_time_us.load())
                            obsolete_completion_us.store(monotonicUs() - request_time_us.load());
                        return false;
                    }
                    preroll_policy.Observe(decode_us);
                    if (framePtr) observed_frame_bytes = std::max(observed_frame_bytes, framePtr->GetBytes());
                    if (!timeline)
                        cache->Add(framePtr);
                    cached_frame_count.store(cache->Count());
                    ++fetched_this_pass;
                }
                catch (const OutOfBoundsFrame&) {
                    break;
                }
                window_full = false;
            }
            else {
                cache->Touch(next_frame);
            }

            {
                std::lock_guard<std::mutex> guard(seek_state_mutex);
                if (threadShouldExit() || request_generation.load() != generation
                    || (timeline && timeline->CacheEpoch() != epoch))
                    return false;
                last_cached_index.store(next_frame);
                if (timeline) {
                    const auto* settings = Settings::Instance();
                    const int64_t available = dir > 0 ? resolveTimelineEnd() - requested_display_frame.load()
                        : requested_display_frame.load() - 1;
                    min_frames_ahead.store(preroll_policy.Target(reader->info.fps.ToDouble(), speed.load(),
                        std::min(readiness_capacity, std::max<int64_t>(0, available)),
                        settings->VIDEO_CACHE_MIN_PREROLL_FRAMES, settings->VIDEO_CACHE_MAX_PREROLL_FRAMES));
                    updateReadiness(cache, requested_display_frame.load(), dir);
                }
                if (diagnostics_enabled.load() && request_time_us.load()
                    && first_current_frame_us.load() == 0
                    && next_frame == clampToTimelineRange(requested_display_frame.load(),
                                                         timeline ? timeline->GetMaxFrame() : reader->info.video_length))
                    first_current_frame_us.store(monotonicUs() - request_time_us.load());
            }
            next_frame += dir;

            // In active playback, avoid long uninterrupted prefetch bursts
            // that can delay player thread frame retrieval.
            if (max_frames_to_fetch > 0 && fetched_this_pass >= max_frames_to_fetch) {
                break;
            }
        }

        return window_full;
    }

    void VideoCacheThread::run()
    {
        while (!threadShouldExit()) {
            Settings* settings = Settings::Instance();
            // Reader replacement waits for this worker before changing the raw
            // pointer. The state lock never covers decoding or timeline clears.
            CacheBase* cache = reader ? reader->GetCache() : nullptr;
            Timeline* timeline = dynamic_cast<Timeline*>(reader);
            uint64_t generation;
            int64_t raw_playhead;
            int dir;
            bool paused, did_user_seek, use_preroll, should_clear;
            {
                std::lock_guard<std::mutex> guard(seek_state_mutex);
                generation = request_generation.load();
                raw_playhead = requested_display_frame.load();
                dir = computeDirection();
                paused = speed.load() == 0;
                did_user_seek = userSeeked.exchange(false);
                use_preroll = preroll_on_next_fill.exchange(false);
                should_clear = clear_cache_on_next_fill.exchange(false);
            }
            // Position changes reset scheduling, not reusable reader caches.
            // Full edit invalidation is deferred by Timeline::RequestClearAllCache.
            if (request_generation.load() != generation)
                continue;

            if (!settings->ENABLE_PLAYBACK_CACHING || !cache) {
                cache_audio_only.store(false);
                min_frames_ahead.store(-1);
                processed_generation.store(generation);
                wait(50);
                continue;
            }

            if (!timeline) {
                // Window prefetch is Timeline-only. Direct FFmpeg/Dummy readers
                // must not wait forever for a cache window this worker won't fill.
                cache_audio_only.store(false);
                min_frames_ahead.store(-1);
                processed_generation.store(generation);
                wait(50);
                continue;
            }
            const int64_t timeline_end = resolveTimelineEnd();
            const int64_t playhead = clampToTimelineRange(raw_playhead, timeline_end);
            const int64_t preroll_frames = computePrerollFrames(settings);
            const int64_t estimated_frame_bytes = getBytes(
                timeline->preview_width ? timeline->preview_width : reader->info.width,
                timeline->preview_height ? timeline->preview_height : reader->info.height,
                reader->info.sample_rate, reader->info.channels, reader->info.fps.ToFloat());
            int64_t bytes_per_frame;
            {
                std::lock_guard<std::mutex> guard(seek_state_mutex);
                if (readiness_bytes_per_frame != estimated_frame_bytes) observed_frame_bytes = 0;
                bytes_per_frame = std::max(estimated_frame_bytes, observed_frame_bytes);
            }
            const int64_t max_bytes = cache->GetMaxBytes();
            const int64_t capacity = (max_bytes > 0 && bytes_per_frame > 0)
                ? std::min<int64_t>(max_bytes / bytes_per_frame, settings->VIDEO_CACHE_MAX_FRAMES)
                : 0;
            const int64_t ahead_count = std::max<int64_t>(0, std::min<int64_t>(capacity - 1, static_cast<int64_t>(capacity * settings->VIDEO_CACHE_PERCENT_AHEAD)));
            int64_t window_begin, window_end;
            computeWindowBounds(playhead, dir, ahead_count, timeline_end, window_begin, window_end);
            {
                std::lock_guard<std::mutex> guard(seek_state_mutex);
                if (request_generation.load() != generation)
                    continue;
                const uint64_t epoch = timeline->CacheEpoch();
                const bool epoch_changed = timeline_cache_epoch_initialized && epoch != seen_timeline_cache_epoch;
                seen_timeline_cache_epoch = epoch;
                timeline_cache_epoch_initialized = true;
                cached_frame_count.store(cache->Count());
                if (epoch_changed || readiness_bytes_per_frame != estimated_frame_bytes) {
                    resetReadiness();
                    observed_frame_bytes = 0;
                }
                readiness_bytes_per_frame = estimated_frame_bytes;
                cache_audio_only.store(capacity >= 1);
                readiness_epoch = epoch;
                readiness_physical_capacity = capacity;
                readiness_timeline_end = timeline_end;
                readiness_capacity = std::max<int64_t>(0, std::min(capacity - 1, ahead_count));
                const int64_t available = dir > 0 ? timeline_end - playhead : playhead - 1;
                readiness_minimum_step = std::min({readiness_capacity, available, std::max<int64_t>(1, std::abs(static_cast<int64_t>(speed.load())))});
                min_frames_ahead.store(preroll_policy.Target(reader->info.fps.ToDouble(), speed.load(),
                    std::min(readiness_capacity, available), settings->VIDEO_CACHE_MIN_PREROLL_FRAMES,
                    settings->VIDEO_CACHE_MAX_PREROLL_FRAMES));
                updateReadiness(cache, playhead, dir);
                // Eviction can punch holes behind the prefetch high-water mark.
                // Repair the first required hole, preserving reusable suffixes.
                const int64_t required = std::max(min_frames_ahead.load(), readiness_minimum_step);
                const bool missing_current = !cache->Contains(playhead);
                if (missing_current || contiguous_ahead < required) {
                    const int64_t missing = missing_current ? playhead
                        : playhead + (contiguous_ahead + 1) * dir;
                    if ((last_cached_index.load() - missing) * dir >= 0)
                        last_cached_index.store(missing - dir);
                }
                if (epoch_changed || should_clear || processed_generation.load() != generation)
                    handleUserSeek(playhead, dir);
                processed_generation.store(generation);
                if (did_user_seek) {
                    if (use_preroll && paused)
                        handleUserSeekWithPreroll(playhead, dir, timeline_end, preroll_frames);
                    else
                        handleUserSeek(playhead, dir);
                } else if (!paused && capacity >= 1) {
                    const bool outside_window = (dir > 0 && last_cached_index.load() > window_end)
                        || (dir < 0 && last_cached_index.load() < window_begin);
                    if (outside_window)
                        handleUserSeek(playhead, dir);
                }
            }
            if (scrub_active.load()) {
                wait(10);
                continue;
            }
            if (capacity < 1) {
                wait(50);
                continue;
            }
            // Preserve reusable frames on position-only seeks. Full edit refresh
            // is requested explicitly and consumed by Timeline's decoder.
            bool window_full = prefetchWindow(cache, window_begin, window_end, dir,
                                               reader, paused ? -1 : 8, generation);
            if (request_generation.load() != generation)
                continue; // The latest request restarts immediately, without sleep.
            if (paused && window_full)
                cache->Touch(playhead);
            const double fps = reader->info.fps.ToFloat();
            wait(fps > 0 ? std::max(1, static_cast<int>(1000.0 / fps / 4.0)) : 10);
        }
    }

} // namespace openshot
