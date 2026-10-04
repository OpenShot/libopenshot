/**
 * @file
 * @brief Unit tests for VideoCacheThread helper methods
 * @author Jonathan Thomas
 *
 * @ref License
 */

// Copyright (c) 2008-2025 OpenShot Studios, LLC
//
// SPDX-License-Identifier: LGPL-3.0-or-later

#include <memory>
#include <future>
#include <chrono>
#include <condition_variable>
#include "openshot_catch.h"

#include "Qt/VideoCacheThread.h"
#include "CacheMemory.h"
#include "ReaderBase.h"
#include "Frame.h"
#include "Settings.h"
#include "FFmpegReader.h"
#include "Timeline.h"
#include "DummyReader.h"
#include "Clip.h"
#include <algorithm>
#include <iostream>
#include <thread>

using namespace openshot;

// ----------------------------------------------------------------------------
// TestableVideoCacheThread: expose protected/internal members for testing
//
class TestableVideoCacheThread : public VideoCacheThread {
public:
    using VideoCacheThread::computeDirection;
    using VideoCacheThread::computeWindowBounds;
    using VideoCacheThread::clearCacheIfPaused;
    using VideoCacheThread::prefetchWindow;
    using VideoCacheThread::handleUserSeek;
    using VideoCacheThread::handleUserSeekWithPreroll;
    using VideoCacheThread::computePrerollFrames;
    void probe(int64_t playhead, int dir, int64_t target, int64_t minimum = 1) {
        std::lock_guard<std::mutex> lock(seek_state_mutex);
        processed_generation.store(request_generation.load());
        readiness_capacity = target;
        readiness_physical_capacity = target + 1;
        readiness_timeline_end = reader->info.video_length;
        readiness_minimum_step = minimum;
        min_frames_ahead.store(target);
        updateReadiness(reader->GetCache(), playhead, dir);
    }

    int64_t getLastCachedIndex() const { return last_cached_index.load(); }
    void    setLastCachedIndex(int64_t v) { last_cached_index.store(v); }
    void    setPlayhead(int64_t v) { requested_display_frame.store(v); }
    void    setMinFramesAhead(int64_t v) { min_frames_ahead.store(v); }
    void    setLastDir(int d) { last_dir.store(d); }
    void    forceUserSeekFlag() { userSeeked.store(true); }
    bool    isScrubbing() const { return scrub_active.load(); }
    bool    getUserSeekedFlag() const { return userSeeked.load(); }
    bool    getPrerollOnNextFill() const { return preroll_on_next_fill.load(); }
    bool    getClearCacheOnNextFill() const { return clear_cache_on_next_fill.load(); }
    int64_t getRequestedDisplayFrame() const { return requested_display_frame.load(); }
};

// ----------------------------------------------------------------------------
// TESTS
// ----------------------------------------------------------------------------

TEST_CASE("computeDirection: respects speed and last_dir", "[VideoCacheThread]") {
    TestableVideoCacheThread thread;

    // Default: speed=0, last_dir initialized to +1
    CHECK(thread.computeDirection() == 1);

    // Positive speed
    thread.setSpeed(3);
    CHECK(thread.computeDirection() == 1);
    CHECK(thread.getSpeed() == 3);

    // Negative speed
    thread.setSpeed(-2);
    CHECK(thread.computeDirection() == -1);
    CHECK(thread.getSpeed() == -2);

    // Pause should preserve last_dir = -1
    thread.setSpeed(0);
    CHECK(thread.computeDirection() == -1);

    // Manually override last_dir to +1, then pause
    thread.setLastDir(1);
    thread.setSpeed(0);
    CHECK(thread.computeDirection() == 1);
}

TEST_CASE("computeWindowBounds: forward and backward bounds, clamped", "[VideoCacheThread]") {
    TestableVideoCacheThread thread;
    int64_t wb, we;

    // Forward direction, normal case
    thread.computeWindowBounds(/*playhead=*/10, /*dir=*/1, /*ahead_count=*/5, /*timeline_end=*/50, wb, we);
    CHECK(wb == 10);
    CHECK(we == 15);

    // Forward direction, at timeline edge
    thread.computeWindowBounds(/*playhead=*/47, /*dir=*/1, /*ahead_count=*/10, /*timeline_end=*/50, wb, we);
    CHECK(wb == 47);
    CHECK(we == 50);  // clamped to 50

    // Backward direction, normal
    thread.computeWindowBounds(/*playhead=*/20, /*dir=*/-1, /*ahead_count=*/7, /*timeline_end=*/100, wb, we);
    CHECK(wb == 13);
    CHECK(we == 20);

    // Backward direction, window_begin < 1
    thread.computeWindowBounds(/*playhead=*/3, /*dir=*/-1, /*ahead_count=*/10, /*timeline_end=*/100, wb, we);
    CHECK(wb == 1);   // clamped
    CHECK(we == 3);
}

TEST_CASE("clearCacheIfPaused: clears only when paused and not in cache", "[VideoCacheThread]") {
    TestableVideoCacheThread thread;
    CacheMemory cache(/*max_bytes=*/100000000);

    // Create a Timeline so that clearCacheIfPaused can call ClearAllCache safely
    Timeline timeline(/*width=*/1280, /*height=*/720, /*fps=*/Fraction(24,1),
                      /*sample_rate=*/48000, /*channels=*/2, ChannelLayout::LAYOUT_STEREO);
    timeline.SetCache(&cache);
    thread.Reader(&timeline);

    // Add a frame so Contains returns true for 5 and 10
    cache.Add(std::make_shared<Frame>(5, 0, 0));
    cache.Add(std::make_shared<Frame>(10, 0, 0));

    // Paused, playhead not in cache → should clear all cache
    bool didClear = thread.clearCacheIfPaused(/*playhead=*/42, /*paused=*/true, &cache);
    CHECK(didClear);
    CHECK(cache.Count() == 0);

    // Re-add a frame for next checks
    cache.Add(std::make_shared<Frame>(5, 0, 0));

    // Paused, but playhead IS in cache → no clear
    didClear = thread.clearCacheIfPaused(/*playhead=*/5, /*paused=*/true, &cache);
    CHECK(!didClear);
    CHECK(cache.Contains(5));

    // Not paused → should not clear even if playhead missing
    didClear = thread.clearCacheIfPaused(/*playhead=*/99, /*paused=*/false, &cache);
    CHECK(!didClear);
    CHECK(cache.Contains(5));
}

TEST_CASE("clearCacheIfPaused: clears when paused past timeline end and playhead frame is missing", "[VideoCacheThread]") {
    TestableVideoCacheThread thread;
    CacheMemory cache(/*max_bytes=*/100000000);

    Timeline timeline(/*width=*/1280, /*height=*/720, /*fps=*/Fraction(24,1),
                      /*sample_rate=*/48000, /*channels=*/2, ChannelLayout::LAYOUT_STEREO);
    timeline.SetCache(&cache);
    thread.Reader(&timeline);

    const int64_t end = timeline.info.video_length;
    REQUIRE(end > 1);

    cache.Add(std::make_shared<Frame>(end - 1, 0, 0));
    const int64_t initial_count = cache.Count();
    REQUIRE(initial_count > 0);

    const bool didClear = thread.clearCacheIfPaused(/*playhead=*/end + 12, /*paused=*/true, &cache);
    CHECK(didClear);
    CHECK(cache.Count() == 0);
}

TEST_CASE("clearCacheIfPaused: does not clear when paused past timeline end and end frame is cached", "[VideoCacheThread]") {
    TestableVideoCacheThread thread;
    CacheMemory cache(/*max_bytes=*/100000000);

    Timeline timeline(/*width=*/1280, /*height=*/720, /*fps=*/Fraction(24,1),
                      /*sample_rate=*/48000, /*channels=*/2, ChannelLayout::LAYOUT_STEREO);
    timeline.SetCache(&cache);
    thread.Reader(&timeline);

    const int64_t end = timeline.info.video_length;
    REQUIRE(end > 1);

    cache.Add(std::make_shared<Frame>(end, 0, 0));
    const int64_t initial_count = cache.Count();
    REQUIRE(initial_count > 0);

    const bool didClear = thread.clearCacheIfPaused(/*playhead=*/end + 12, /*paused=*/true, &cache);
    CHECK(!didClear);
    CHECK(cache.Count() == initial_count);
}

TEST_CASE("handleUserSeek: sets last_cached_index to playhead - dir", "[VideoCacheThread]") {
    TestableVideoCacheThread thread;

    thread.setLastCachedIndex(100);
    thread.handleUserSeek(/*playhead=*/50, /*dir=*/1);
    CHECK(thread.getLastCachedIndex() == 49);

    thread.handleUserSeek(/*playhead=*/50, /*dir=*/-1);
    CHECK(thread.getLastCachedIndex() == 51);
}

TEST_CASE("handleUserSeekWithPreroll: offsets start by preroll frames", "[VideoCacheThread]") {
    TestableVideoCacheThread thread;

    thread.handleUserSeekWithPreroll(/*playhead=*/60, /*dir=*/1, /*timeline_end=*/200, /*preroll_frames=*/30);
    CHECK(thread.getLastCachedIndex() == 29);

    thread.handleUserSeekWithPreroll(/*playhead=*/10, /*dir=*/1, /*timeline_end=*/200, /*preroll_frames=*/30);
    CHECK(thread.getLastCachedIndex() == 0);

    thread.handleUserSeekWithPreroll(/*playhead=*/1, /*dir=*/1, /*timeline_end=*/200, /*preroll_frames=*/30);
    CHECK(thread.getLastCachedIndex() == 0);

    thread.handleUserSeekWithPreroll(/*playhead=*/60, /*dir=*/-1, /*timeline_end=*/200, /*preroll_frames=*/30);
    CHECK(thread.getLastCachedIndex() == 91);
}

TEST_CASE("prefetchWindow: forward caching with FFmpegReader & CacheMemory", "[VideoCacheThread]") {
    TestableVideoCacheThread thread;
    CacheMemory cache(/*max_bytes=*/100000000);

    // Use a real test file via FFmpegReader
    std::string path = std::string(TEST_MEDIA_PATH) + "sintel_trailer-720p.mp4";
    FFmpegReader reader(path);
    reader.Open();

    // Setup: window [1..5], dir=1, last_cached_index=0
    thread.setLastCachedIndex(0);
    int64_t window_begin = 1, window_end = 5;

    bool wasFull = thread.prefetchWindow(&cache, window_begin, window_end, /*dir=*/1, &reader);
    CHECK(!wasFull);

    // Should have cached frames 1..5
    CHECK(thread.getLastCachedIndex() == window_end);
    for (int64_t f = 1; f <= 5; ++f) {
        CHECK(cache.Contains(f));
    }

    // Now window is full; next prefetch should return true
    wasFull = thread.prefetchWindow(&cache, window_begin, window_end, /*dir=*/1, &reader);
    CHECK(wasFull);
    CHECK(thread.getLastCachedIndex() == window_end);
}

TEST_CASE("prefetchWindow: backward caching with FFmpegReader & CacheMemory", "[VideoCacheThread]") {
    TestableVideoCacheThread thread;
    CacheMemory cache(/*max_bytes=*/100000000);

    // Use a real test file via FFmpegReader
    std::string path = std::string(TEST_MEDIA_PATH) + "sintel_trailer-720p.mp4";
    FFmpegReader reader(path);
    reader.Open();

    // Setup: window [10..15], dir=-1, last_cached_index=16
    thread.setLastCachedIndex(16);
    int64_t window_begin = 10, window_end = 15;

    bool wasFull = thread.prefetchWindow(&cache, window_begin, window_end, /*dir=*/-1, &reader);
    CHECK(!wasFull);

    // Should have cached frames 15..10
    CHECK(thread.getLastCachedIndex() == window_begin);
    for (int64_t f = 10; f <= 15; ++f) {
        CHECK(cache.Contains(f));
    }

    // Next call should return true
    wasFull = thread.prefetchWindow(&cache, window_begin, window_end, /*dir=*/-1, &reader);
    CHECK(wasFull);
    CHECK(thread.getLastCachedIndex() == window_begin);
}

TEST_CASE("prefetchWindow: interrupt on userSeeked flag", "[VideoCacheThread]") {
    TestableVideoCacheThread thread;
    CacheMemory cache(/*max_bytes=*/100000000);

    // Use a real test file via FFmpegReader
    std::string path = std::string(TEST_MEDIA_PATH) + "sintel_trailer-720p.mp4";
    FFmpegReader reader(path);
    reader.Open();

    // Window [20..30], dir=1, last_cached_index=19
    thread.setLastCachedIndex(19);
    int64_t window_begin = 20, window_end = 30;

    // Subclass CacheMemory to interrupt on frame 23
    class InterruptingCache : public CacheMemory {
    public:
        TestableVideoCacheThread* tcb;
        InterruptingCache(int64_t maxb, TestableVideoCacheThread* t)
            : CacheMemory(maxb), tcb(t) {}
        void Add(std::shared_ptr<openshot::Frame> frame) override {
            int64_t idx = frame->number;  // use public member 'number'
            CacheMemory::Add(frame);
            if (idx == 23) {
                tcb->forceUserSeekFlag();
            }
        }
    } interruptingCache(/*max_bytes=*/100000000, &thread);

    bool wasFull = thread.prefetchWindow(&interruptingCache,
                                          window_begin,
                                          window_end,
                                          /*dir=*/1,
                                          &reader);

    // Should stop at 23
    CHECK(thread.getLastCachedIndex() == 23);
    CHECK(!wasFull);
}

TEST_CASE("Seek preview: preserves playhead frame when paused and inside cache", "[VideoCacheThread]") {
    TestableVideoCacheThread thread;
    CacheMemory cache(/*max_bytes=*/100000000);
    Timeline timeline(/*width=*/1280, /*height=*/720, /*fps=*/Fraction(24,1),
                      /*sample_rate=*/48000, /*channels=*/2, ChannelLayout::LAYOUT_STEREO);
    timeline.SetCache(&cache);
    thread.Reader(&timeline);

    cache.Add(std::make_shared<Frame>(100, 0, 0));
    cache.Add(std::make_shared<Frame>(101, 0, 0));
    REQUIRE(cache.Count() >= 2);

    thread.Seek(/*new_position=*/100, /*start_preroll=*/false);

    CHECK(thread.isScrubbing());
    CHECK(!thread.getUserSeekedFlag());
    CHECK(!thread.getPrerollOnNextFill());
    CHECK(cache.Contains(100));
    CHECK(cache.Count() >= 2);
}

TEST_CASE("Seek preview: outside cache marks uncached without preroll", "[VideoCacheThread]") {
    TestableVideoCacheThread thread;
    CacheMemory cache(/*max_bytes=*/100000000);
    Timeline timeline(/*width=*/1280, /*height=*/720, /*fps=*/Fraction(24,1),
                      /*sample_rate=*/48000, /*channels=*/2, ChannelLayout::LAYOUT_STEREO);
    timeline.SetCache(&cache);
    thread.Reader(&timeline);

    cache.Add(std::make_shared<Frame>(10, 0, 0));
    cache.Add(std::make_shared<Frame>(11, 0, 0));
    REQUIRE(cache.Count() >= 2);

    thread.Seek(/*new_position=*/300, /*start_preroll=*/false);

    CHECK(thread.isScrubbing());
    CHECK(thread.getUserSeekedFlag());
    CHECK(!thread.getPrerollOnNextFill());
    CHECK(cache.Count() >= 2);
}

TEST_CASE("Seek commit: exits scrub mode and enables preroll when uncached", "[VideoCacheThread]") {
    TestableVideoCacheThread thread;
    CacheMemory cache(/*max_bytes=*/100000000);
    Timeline timeline(/*width=*/1280, /*height=*/720, /*fps=*/Fraction(24,1),
                      /*sample_rate=*/48000, /*channels=*/2, ChannelLayout::LAYOUT_STEREO);
    timeline.SetCache(&cache);
    thread.Reader(&timeline);

    thread.Seek(/*new_position=*/200, /*start_preroll=*/false);
    CHECK(thread.isScrubbing());

    thread.Seek(/*new_position=*/200, /*start_preroll=*/true);

    CHECK(!thread.isScrubbing());
    CHECK(thread.getUserSeekedFlag());
    CHECK(thread.getPrerollOnNextFill());
}

TEST_CASE("Seek commit: paused in-range seek preserves cached window state", "[VideoCacheThread]") {
    TestableVideoCacheThread thread;
    CacheMemory cache(/*max_bytes=*/100000000);
    Timeline timeline(/*width=*/1280, /*height=*/720, /*fps=*/Fraction(24,1),
                      /*sample_rate=*/48000, /*channels=*/2, ChannelLayout::LAYOUT_STEREO);
    timeline.SetCache(&cache);
    thread.Reader(&timeline);

    cache.Add(std::make_shared<Frame>(120, 0, 0));
    cache.Add(std::make_shared<Frame>(121, 0, 0));
    REQUIRE(cache.Count() >= 2);

    // Simulate existing cache progress so we can verify no baseline reset.
    thread.setLastCachedIndex(180);

    thread.Seek(/*new_position=*/120, /*start_preroll=*/true);

    CHECK(!thread.isScrubbing());
    CHECK(!thread.getUserSeekedFlag());
    CHECK(!thread.getPrerollOnNextFill());
    CHECK(thread.getLastCachedIndex() == 180);
    CHECK(cache.Contains(120));
}

TEST_CASE("Seek commit: paused scrub preview then same-frame commit preserves cache", "[VideoCacheThread]") {
    TestableVideoCacheThread thread;
    CacheMemory cache(/*max_bytes=*/100000000);
    Timeline timeline(/*width=*/1280, /*height=*/720, /*fps=*/Fraction(24,1),
                      /*sample_rate=*/48000, /*channels=*/2, ChannelLayout::LAYOUT_STEREO);
    timeline.SetCache(&cache);
    thread.Reader(&timeline);

    cache.Add(std::make_shared<Frame>(140, 0, 0));
    cache.Add(std::make_shared<Frame>(141, 0, 0));
    REQUIRE(cache.Count() >= 2);

    thread.setLastCachedIndex(210);

    // Typical paused seek flow: preview move, then commit same frame.
    thread.Seek(/*new_position=*/140, /*start_preroll=*/false);
    REQUIRE(thread.isScrubbing());
    REQUIRE(thread.getRequestedDisplayFrame() == 140);

    thread.Seek(/*new_position=*/140, /*start_preroll=*/true);

    CHECK(!thread.isScrubbing());
    CHECK(!thread.getUserSeekedFlag());
    CHECK(!thread.getPrerollOnNextFill());
    CHECK(thread.getLastCachedIndex() == 210);
    CHECK(cache.Contains(140));
    CHECK(cache.Count() >= 2);
}

TEST_CASE("Seek preview: paused out-of-range seek clamps to end and preserves cache", "[VideoCacheThread]") {
    TestableVideoCacheThread thread;
    CacheMemory cache(/*max_bytes=*/100000000);
    Timeline timeline(/*width=*/1280, /*height=*/720, /*fps=*/Fraction(24,1),
                      /*sample_rate=*/48000, /*channels=*/2, ChannelLayout::LAYOUT_STEREO);
    timeline.SetCache(&cache);
    thread.Reader(&timeline);

    const int64_t end = timeline.info.video_length;
    REQUIRE(end > 1);

    cache.Add(std::make_shared<Frame>(end, 0, 0));
    cache.Add(std::make_shared<Frame>(end - 1, 0, 0));
    thread.setLastCachedIndex(end - 1);

    thread.Seek(/*new_position=*/end + 24, /*start_preroll=*/false);

    CHECK(thread.isScrubbing());
    CHECK(!thread.getUserSeekedFlag());
    CHECK(thread.getRequestedDisplayFrame() == end + 24);
    CHECK(thread.getLastCachedIndex() == end - 1);
    CHECK(cache.Contains(end));
}

TEST_CASE("Seek commit: paused out-of-range seek past end enables cache rebuild when end is uncached", "[VideoCacheThread]") {
    TestableVideoCacheThread thread;
    CacheMemory cache(/*max_bytes=*/100000000);
    Timeline timeline(/*width=*/1280, /*height=*/720, /*fps=*/Fraction(24,1),
                      /*sample_rate=*/48000, /*channels=*/2, ChannelLayout::LAYOUT_STEREO);
    timeline.SetCache(&cache);
    thread.Reader(&timeline);

    const int64_t end = timeline.info.video_length;
    REQUIRE(end > 1);

    cache.Add(std::make_shared<Frame>(1, 0, 0));
    cache.Add(std::make_shared<Frame>(2, 0, 0));
    thread.setLastCachedIndex(2);

    thread.Seek(/*new_position=*/end + 24, /*start_preroll=*/true);

    CHECK(!thread.isScrubbing());
    CHECK(thread.getUserSeekedFlag());
    CHECK(thread.getPrerollOnNextFill());
    CHECK(thread.getRequestedDisplayFrame() == end + 24);
    CHECK(thread.getLastCachedIndex() == end - 1);
    CHECK(!cache.Contains(end));
}

TEST_CASE("Seek commit: playback jump to cached frame preserves cache", "[VideoCacheThread]") {
    TestableVideoCacheThread thread;
    CacheMemory cache(/*max_bytes=*/100000000);
    Timeline timeline(/*width=*/1280, /*height=*/720, /*fps=*/Fraction(24,1),
                      /*sample_rate=*/48000, /*channels=*/2, ChannelLayout::LAYOUT_STEREO);
    timeline.SetCache(&cache);
    thread.Reader(&timeline);

    cache.Add(std::make_shared<Frame>(150, 0, 0));
    cache.Add(std::make_shared<Frame>(220, 0, 0));
    thread.setPlayhead(220);
    thread.setSpeed(1);
    thread.setLastCachedIndex(230);

    thread.Seek(/*new_position=*/150, /*start_preroll=*/true);

    CHECK(!thread.isScrubbing());
    CHECK(!thread.getUserSeekedFlag());
    CHECK(!thread.getPrerollOnNextFill());
    CHECK(!thread.getClearCacheOnNextFill());
    CHECK(thread.getRequestedDisplayFrame() == 150);
    CHECK(thread.getLastCachedIndex() == 230);
}

TEST_CASE("Seek commit: playback click inside active cached window preserves cache", "[VideoCacheThread]") {
    TestableVideoCacheThread thread;
    CacheMemory cache(/*max_bytes=*/100000000);
    Timeline timeline(/*width=*/1280, /*height=*/720, /*fps=*/Fraction(24,1),
                      /*sample_rate=*/48000, /*channels=*/2, ChannelLayout::LAYOUT_STEREO);
    timeline.SetCache(&cache);
    thread.Reader(&timeline);

    cache.Add(std::make_shared<Frame>(220, 0, 0));
    cache.Add(std::make_shared<Frame>(230, 0, 0));
    cache.Add(std::make_shared<Frame>(260, 0, 0));
    thread.setPlayhead(220);
    thread.setSpeed(1);
    thread.setLastCachedIndex(260);

    thread.Seek(/*new_position=*/230, /*start_preroll=*/true);

    CHECK(!thread.isScrubbing());
    CHECK(!thread.getUserSeekedFlag());
    CHECK(!thread.getPrerollOnNextFill());
    CHECK(!thread.getClearCacheOnNextFill());
    CHECK(thread.getRequestedDisplayFrame() == 230);
    CHECK(thread.getLastCachedIndex() == 260);
}

TEST_CASE("NotifyPlaybackPosition: ignored while scrubbing, applied after commit", "[VideoCacheThread]") {
    TestableVideoCacheThread thread;
    CacheMemory cache(/*max_bytes=*/100000000);
    Timeline timeline(/*width=*/1280, /*height=*/720, /*fps=*/Fraction(24,1),
                      /*sample_rate=*/48000, /*channels=*/2, ChannelLayout::LAYOUT_STEREO);
    timeline.SetCache(&cache);
    thread.Reader(&timeline);

    thread.Seek(/*new_position=*/120, /*start_preroll=*/false);
    REQUIRE(thread.isScrubbing());
    CHECK(thread.getRequestedDisplayFrame() == 120);

    thread.NotifyPlaybackPosition(/*new_position=*/25);
    CHECK(thread.getRequestedDisplayFrame() == 120);

    thread.Seek(/*new_position=*/120, /*start_preroll=*/true);
    REQUIRE(!thread.isScrubbing());

    thread.NotifyPlaybackPosition(/*new_position=*/25);
    CHECK(thread.getRequestedDisplayFrame() == 25);
}

TEST_CASE("Seek non-preroll: playback uncached target does not force cache rebuild", "[VideoCacheThread]") {
    TestableVideoCacheThread thread;
    CacheMemory cache(/*max_bytes=*/100000000);
    Timeline timeline(/*width=*/1280, /*height=*/720, /*fps=*/Fraction(24,1),
                      /*sample_rate=*/48000, /*channels=*/2, ChannelLayout::LAYOUT_STEREO);
    timeline.SetCache(&cache);
    thread.Reader(&timeline);

    cache.Add(std::make_shared<Frame>(220, 0, 0));
    cache.Add(std::make_shared<Frame>(221, 0, 0));
    thread.setPlayhead(220);
    thread.setSpeed(1);
    thread.setLastCachedIndex(230);

    thread.Seek(/*new_position=*/120, /*start_preroll=*/false);

    CHECK(!thread.isScrubbing());
    CHECK(!thread.getUserSeekedFlag());
    CHECK(!thread.getPrerollOnNextFill());
    CHECK(!thread.getClearCacheOnNextFill());
    CHECK(thread.getRequestedDisplayFrame() == 120);
    CHECK(thread.getLastCachedIndex() == 230);
}

// A deterministic decoder boundary: release explicitly instead of relying on
// machine-dependent decode durations. It also models Timeline's internal Add.
class GatedCacheTimeline : public Timeline {
public:
    GatedCacheTimeline() : Timeline(16, 16, Fraction(30, 1), 48000, 2, LAYOUT_STEREO) {}
    std::promise<void> entered;
    std::promise<void> release;
    std::promise<int64_t> second_frame;
    std::shared_future<void> released = release.get_future().share();
    std::atomic<int> calls{0};
    std::shared_ptr<Frame> GetFrame(int64_t number) override {
        const int call = calls.fetch_add(1);
        if (call == 0) {
            entered.set_value();
            released.wait();
        } else if (call == 1) {
            second_frame.set_value(number);
        }
        auto frame = std::make_shared<Frame>(number, 16, 16, "black", 0, 2);
        GetCache()->Add(frame);
        return frame;
    }
    std::unique_lock<std::recursive_mutex> holdDecoder() {
        return std::unique_lock<std::recursive_mutex>(getFrameMutex);
    }
};

TEST_CASE("explicit seek supersedes an in-flight cache fill", "[VideoCacheThread][cancellation]") {
    GatedCacheTimeline timeline;
    TestableVideoCacheThread thread;
    thread.Reader(&timeline);
    thread.setSpeed(1);
    thread.setLastCachedIndex(0);
    auto entered = timeline.entered.get_future();
    auto fill = std::async(std::launch::async, [&] {
        return thread.prefetchWindow(timeline.GetCache(), 1, 20, 1, &timeline);
    });
    const bool decoder_entered = entered.wait_for(std::chrono::seconds(2)) == std::future_status::ready;
    if (!decoder_entered) {
        timeline.release.set_value();
        fill.get();
    }
    REQUIRE(decoder_entered);
    thread.Seek(90, false); // Live seek did not set userSeeked in the old worker.
    const auto baseline = thread.getLastCachedIndex();
    timeline.release.set_value();
    fill.get();
    CHECK(timeline.calls.load() == 1);
    CHECK(thread.getLastCachedIndex() == baseline);
    // A position change alone keeps valid reusable reader-owned cached frames.
    CHECK(timeline.GetCache()->Contains(1));
}

TEST_CASE("paused edit refresh acknowledges without waiting for decoder", "[VideoCacheThread][cancellation]") {
    GatedCacheTimeline timeline;
    TestableVideoCacheThread thread;
    thread.Reader(&timeline);
    auto decoder_lock = timeline.holdDecoder();
    auto seek = std::async(std::launch::async, [&] { thread.Seek(1, true); });
    const auto acknowledgement = seek.wait_for(std::chrono::milliseconds(100));
    decoder_lock.unlock();
    seek.get();
    CHECK(acknowledgement == std::future_status::ready);
}

TEST_CASE("timeline deferred edit refresh bypasses cached frame and survives repeated requests", "[VideoCacheThread][cancellation]") {
    GatedCacheTimeline timeline;
    timeline.Open();
    auto cache = timeline.GetCache();
    for (int i = 0; i < 100; ++i) {
        auto old = std::make_shared<Frame>(1, 16, 16, "red", 0, 2);
        cache->Clear();
        cache->Add(old);
        timeline.RequestClearAllCache();
        timeline.RequestClearAllCache();
        auto current = timeline.Timeline::GetFrame(1);
        REQUIRE(current != old);
        CHECK(cache->GetFrame(1) == current);
    }
}

TEST_CASE("cache stop timeout drains cooperatively and reader replacement waits for ownership", "[VideoCacheThread][cancellation]") {
    GatedCacheTimeline timeline;
    Timeline next(16, 16, Fraction(30, 1), 48000, 2, LAYOUT_STEREO);
    TestableVideoCacheThread thread;
    thread.Reader(&timeline);
    thread.setSpeed(1);
    auto entered = timeline.entered.get_future();
    thread.StartThread();
    const bool decoder_entered = entered.wait_for(std::chrono::seconds(2)) == std::future_status::ready;
    if (!decoder_entered) {
        timeline.release.set_value();
        thread.StopThread(-1);
    }
    REQUIRE(decoder_entered);
    CHECK_FALSE(thread.StopThread(0));
    CHECK(thread.isThreadRunning());
    auto replacement = std::async(std::launch::async, [&] { thread.Reader(&next); });
    CHECK(replacement.wait_for(std::chrono::milliseconds(20)) == std::future_status::timeout);
    timeline.release.set_value();
    replacement.get();
    CHECK(timeline.calls.load() == 1);
    CHECK(thread.StopThread(-1));
    CHECK(thread.StartThread());
    CHECK(thread.StopThread(-1));
}

class GatedCacheReader : public DummyReader {
public:
    GatedCacheReader() : DummyReader(Fraction(30, 1), 16, 16, 48000, 2, 30) {}
    std::promise<void> entered;
    std::promise<void> release;
    std::shared_future<void> released = release.get_future().share();
    std::atomic<bool> first{true};
    std::shared_ptr<Frame> GetFrame(int64_t number) override {
        if (first.exchange(false)) {
            entered.set_value();
            released.wait();
        }
        return DummyReader::GetFrame(number);
    }
};

TEST_CASE("timeline does not self-publish a decode invalidated during reader work", "[VideoCacheThread][cancellation]") {
    GatedCacheReader source;
    Clip clip(&source);
    clip.End(30);
    Timeline timeline(16, 16, Fraction(30, 1), 48000, 2, LAYOUT_STEREO);
    timeline.AutoMapClips(false);
    timeline.AddClip(&clip);
    timeline.Open();
    auto entered = source.entered.get_future();
    auto decode = std::async(std::launch::async, [&] { return timeline.GetFrame(1); });
    const bool decoder_entered = entered.wait_for(std::chrono::seconds(2)) == std::future_status::ready;
    if (!decoder_entered) {
        source.release.set_value();
        decode.get();
    }
    REQUIRE(decoder_entered);
    for (int i = 0; i < 100; ++i)
        timeline.RequestClearAllCache();
    source.release.set_value();
    decode.get();
    CHECK_FALSE(timeline.GetCache()->Contains(1));
    CHECK(timeline.CacheRefreshPending());
    auto current = timeline.GetFrame(1);
    CHECK_FALSE(timeline.CacheRefreshPending());
    CHECK(timeline.GetCache()->GetFrame(1) == current);
}

TEST_CASE("rapid seeks coalesce into one current cache request", "[VideoCacheThread][cancellation]") {
    GatedCacheTimeline timeline;
    TestableVideoCacheThread thread;
    thread.Reader(&timeline);
    thread.setSpeed(1);
    thread.EnableRequestDiagnostics(true);
    auto entered = timeline.entered.get_future();
    auto next_frame = timeline.second_frame.get_future();
    thread.StartThread();
    const bool decoder_entered = entered.wait_for(std::chrono::seconds(2)) == std::future_status::ready;
    if (!decoder_entered) {
        timeline.release.set_value();
        thread.StopThread(-1);
    }
    REQUIRE(decoder_entered);
    std::vector<int64_t> acknowledgement;
    for (int i = 0; i < 1000; ++i) {
        thread.Seek(i % 2 ? 200 : 20, true);
        acknowledgement.push_back(thread.CancellationAcknowledgementUs());
    }
    thread.Seek(90, true);
    timeline.release.set_value();
    const bool current_entered = next_frame.wait_for(std::chrono::seconds(2)) == std::future_status::ready;
    thread.StopThread(-1);
    REQUIRE(current_entered);
    CHECK(next_frame.get() == 90);
    std::sort(acknowledgement.begin(), acknowledgement.end());
    std::cout << "Cache request acknowledgement (us), n=1000: p50=" << acknowledgement[500]
              << " p95=" << acknowledgement[950] << " p99=" << acknowledgement[990]
              << " max=" << acknowledgement.back() << '\n';
}

TEST_CASE("direct readers do not wait for unsupported timeline prefetch", "[VideoCacheThread][cancellation]") {
    DummyReader reader(Fraction(30, 1), 16, 16, 48000, 2, 30);
    reader.Open();
    TestableVideoCacheThread thread;
    thread.Reader(&reader);
    thread.setSpeed(1);
    auto wait_ready = [&] {
        const auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds(2);
        while (!thread.isReady() && std::chrono::steady_clock::now() < deadline)
            std::this_thread::sleep_for(std::chrono::milliseconds(1));
        return thread.isReady();
    };
    CHECK(thread.StartThread());
    CHECK(wait_ready());
    thread.Seek(90, true);
    CHECK(wait_ready());
    CHECK(thread.StopThread(-1));
    CHECK(thread.StartThread());
    CHECK(wait_ready());
    CHECK(thread.StopThread(-1));
}

TEST_CASE("readiness requires actual contiguous frames and detects eviction", "[VideoCacheThread][preroll]") {
    Timeline timeline(16, 16, Fraction(30,1), 48000, 2, LAYOUT_STEREO);
    TestableVideoCacheThread thread;
    thread.Reader(&timeline);
    thread.setSpeed(1);
    thread.setPlayhead(20);
    auto add = [&](int64_t n) { timeline.GetCache()->Add(std::make_shared<Frame>(n,16,16,"black",0,2)); };
    add(20); add(22); add(23);
    thread.setLastCachedIndex(100); // A distant high-water mark does not cover holes.
    thread.probe(20, 1, 3);
    CHECK_FALSE(thread.isReady());
    add(21);
    thread.probe(20, 1, 3);
    CHECK(thread.isReady());
    timeline.GetCache()->Remove(21);
    CHECK_FALSE(thread.isReady());
    add(21);
    CHECK(thread.isReady());
    thread.setSpeed(-1);
    thread.probe(20, -1, 3);
    CHECK_FALSE(thread.isReady());
    add(19); add(18); add(17);
    thread.probe(20, -1, 3);
    CHECK(thread.isReady());
    thread.Seek(20, true);
    CHECK_FALSE(thread.isReady());
    thread.probe(20, -1, 3);
    CHECK_FALSE(thread.isReady()); // Same-frame playing refresh removes current frame.
    add(20);
    thread.probe(20, -1, 3);
    CHECK(thread.isReady());
    timeline.RequestClearAllCache();
    CHECK_FALSE(thread.isReady());
}
TEST_CASE("readiness clamps to zero steps at timeline boundaries", "[VideoCacheThread][preroll]") {
    Timeline timeline(16,16,Fraction(30,1),48000,2,LAYOUT_STEREO);
    TestableVideoCacheThread thread;
    thread.Reader(&timeline);
    thread.setSpeed(-1);
    thread.setPlayhead(1);
    timeline.GetCache()->Add(std::make_shared<Frame>(1,16,16,"black",0,2));
    thread.probe(1, -1, 0, 0);
    CHECK(thread.isReady());
}

TEST_CASE("one-frame readiness never bypasses an evicted current frame", "[VideoCacheThread][preroll]") {
    Timeline timeline(16,16,Fraction(30,1),48000,2,LAYOUT_STEREO);
    TestableVideoCacheThread thread;
    thread.Reader(&timeline);
    thread.setSpeed(1);
    thread.setPlayhead(1);
    thread.probe(1,1,0,0);
    CHECK_FALSE(thread.isReady());
    timeline.GetCache()->Add(std::make_shared<Frame>(1,16,16,"black",0,2));
    CHECK(thread.isReady());
    timeline.GetCache()->Remove(1);
    CHECK_FALSE(thread.isReady());
}

TEST_CASE("worker readiness handles disabled zero and one-frame capacity", "[VideoCacheThread][preroll]") {
    auto* settings = Settings::Instance();
    struct Restore {
        Settings* s; bool enabled; int frames; float ahead;
        ~Restore() { s->ENABLE_PLAYBACK_CACHING=enabled; s->VIDEO_CACHE_MAX_FRAMES=frames; s->VIDEO_CACHE_PERCENT_AHEAD=ahead; }
    } restore{settings,settings->ENABLE_PLAYBACK_CACHING,settings->VIDEO_CACHE_MAX_FRAMES,settings->VIDEO_CACHE_PERCENT_AHEAD};
    Timeline timeline(16,16,Fraction(30,1),48000,2,LAYOUT_STEREO);
    timeline.Open();
    auto ready = [&](TestableVideoCacheThread& thread) {
        const auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds(2);
        while (!thread.isReady() && std::chrono::steady_clock::now() < deadline)
            std::this_thread::sleep_for(std::chrono::milliseconds(1));
        return thread.isReady();
    };
    settings->ENABLE_PLAYBACK_CACHING = false;
    {
        TestableVideoCacheThread thread;
        thread.Reader(&timeline); thread.setSpeed(1);
        CHECK(thread.isReady());
        CHECK_FALSE(thread.UsesCachedAudio());
    }
    settings->ENABLE_PLAYBACK_CACHING = true;
    settings->VIDEO_CACHE_MAX_FRAMES = 0;
    {
        TestableVideoCacheThread thread;
        thread.Reader(&timeline); thread.setSpeed(1); thread.StartThread();
        CHECK(ready(thread));
        CHECK_FALSE(thread.UsesCachedAudio());
        thread.StopThread(-1);
    }
    settings->VIDEO_CACHE_MAX_FRAMES = 1;
    settings->VIDEO_CACHE_PERCENT_AHEAD = 0;
    {
        TestableVideoCacheThread thread;
        thread.Reader(&timeline); thread.setSpeed(1); thread.StartThread();
        CHECK(ready(thread));
        CHECK(timeline.GetCache()->Contains(1));
        CHECK(thread.UsesCachedAudio());
        thread.StopThread(-1);
    }
}
TEST_CASE("readiness clamps out-of-range requests to available endpoint", "[VideoCacheThread][preroll]") {
    Timeline timeline(16,16,Fraction(30,1),48000,2,LAYOUT_STEREO);
    TestableVideoCacheThread thread;
    thread.Reader(&timeline); thread.setSpeed(1);
    const auto end = timeline.info.video_length;
    thread.setPlayhead(end + 100);
    thread.probe(end,1,0,0);
    CHECK_FALSE(thread.isReady());
    timeline.GetCache()->Add(std::make_shared<Frame>(end,16,16,"black",0,2));
    CHECK(thread.isReady());
}

TEST_CASE("worker repairs evicted readiness holes behind its cached suffix", "[VideoCacheThread][preroll]") {
    class RepairTimeline : public Timeline {
    public:
        std::atomic<int> recovered{0};
        std::atomic<bool> count_repair{false};
        RepairTimeline() : Timeline(16,16,Fraction(30,1),48000,2,LAYOUT_STEREO) {}
        std::shared_ptr<Frame> GetFrame(int64_t n) override {
            if (count_repair.load() && (n==20 || n==21)) ++recovered;
            auto frame = std::make_shared<Frame>(n,16,16,"black",0,2);
            GetCache()->Add(frame);
            return frame;
        }
    } timeline;
    TestableVideoCacheThread thread;
    thread.Reader(&timeline); thread.setPlayhead(20); thread.setSpeed(1);
    thread.StartThread();
    const auto warm_deadline = std::chrono::steady_clock::now() + std::chrono::seconds(2);
    while (thread.getLastCachedIndex() < 100 && std::chrono::steady_clock::now() < warm_deadline)
        std::this_thread::sleep_for(std::chrono::milliseconds(1));
    const bool warmed = thread.getLastCachedIndex() >= 100;
    if (!warmed) thread.StopThread(-1);
    REQUIRE(warmed);
    const auto epoch = timeline.CacheEpoch();
    timeline.count_repair.store(true);
    timeline.GetCache()->Remove(20);
    timeline.GetCache()->Remove(21);
    const auto repair_deadline = std::chrono::steady_clock::now() + std::chrono::seconds(2);
    while ((timeline.recovered.load() < 2 || !thread.isReady())
           && std::chrono::steady_clock::now() < repair_deadline)
        std::this_thread::sleep_for(std::chrono::milliseconds(1));
    const bool ready_after_repair = thread.isReady();
    thread.StopThread(-1);
    CHECK(ready_after_repair);
    CHECK(timeline.CacheEpoch() == epoch);
    CHECK(timeline.recovered.load() >= 2);
    CHECK(timeline.GetCache()->Contains(20));
    CHECK(timeline.GetCache()->Contains(21));
    CHECK(timeline.GetCache()->Contains(100)); // Reusable suffix retained.
}
