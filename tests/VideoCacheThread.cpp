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
#include "openshot_catch.h"

#include "Qt/VideoCacheThread.h"
#include "CacheMemory.h"
#include "ReaderBase.h"
#include "Frame.h"
#include "Settings.h"
#include "FFmpegReader.h"
#include "Timeline.h"

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

    int64_t getLastCachedIndex() const { return last_cached_index; }
    void    setLastCachedIndex(int64_t v) { last_cached_index = v; }
    void    setLastDir(int d) { last_dir = d; }
    void    forceUserSeekFlag() { userSeeked = true; }
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

TEST_CASE("handleUserSeek: sets last_cached_index to playhead - dir", "[VideoCacheThread]") {
    TestableVideoCacheThread thread;

    thread.setLastCachedIndex(100);
    thread.handleUserSeek(/*playhead=*/50, /*dir=*/1);
    CHECK(thread.getLastCachedIndex() == 49);

    thread.handleUserSeek(/*playhead=*/50, /*dir=*/-1);
    CHECK(thread.getLastCachedIndex() == 51);
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
