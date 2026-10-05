// SPDX-FileCopyrightText: 2026 OpenShot Studios, LLC
//
// SPDX-License-Identifier: LGPL-3.0-or-later
#include "openshot_catch.h"
#include "AudioReaderSource.h"
#include "DummyReader.h"
#include "Frame.h"
#include "Timeline.h"
#include "CacheBase.h"
#include <condition_variable>
#include <future>
#include <mutex>

namespace {
class LevelReader : public openshot::DummyReader {
public:
    std::mutex mutex;
    std::condition_variable condition;
    bool block = false, entered = false, released = false;
    LevelReader() : DummyReader(openshot::Fraction(30, 1), 16, 16, 48000, 1, 1) {
        info.has_audio = true;
    }
    void Release() {
        std::lock_guard<std::mutex> lock(mutex);
        released = true;
        condition.notify_all();
    }
    std::shared_ptr<openshot::Frame> GetFrame(int64_t number) override {
        if (block) {
            std::unique_lock<std::mutex> lock(mutex);
            entered = true;
            condition.notify_all();
            condition.wait(lock, [this] { return released; });
        }
        auto frame = std::make_shared<openshot::Frame>(number, 8, 1);
        const float level = number == 1 ? 0.25f : -0.5f;
        float samples[8];
        std::fill(samples, samples + 8, level);
        frame->AddAudio(true, 0, 0, samples, 8, 1.0f);
        return frame;
    }
};
}

TEST_CASE("AudioReaderSource replaces only the requested buffer region", "[audio-source]") {
    LevelReader reader;
    openshot::AudioReaderSource source(&reader, 1);
    juce::AudioBuffer<float> buffer(2, 12);
    for (int channel = 0; channel < 2; ++channel)
        for (int sample = 0; sample < 12; ++sample)
            buffer.setSample(channel, sample, 0.75f);
    source.getNextAudioBlock({&buffer, 2, 4});
    CHECK(buffer.getSample(0, 2) == Approx(0.25f));
    CHECK(buffer.getSample(1, 2) == Approx(0.0f));
    CHECK(buffer.getSample(0, 0) == Approx(0.75f));
    CHECK(buffer.getSample(1, 8) == Approx(0.75f));
    source.setSpeed(0);
    source.getNextAudioBlock({&buffer, 2, 4});
    CHECK(buffer.getSample(0, 2) == Approx(0.0f));
    CHECK(buffer.getSample(0, 0) == Approx(0.75f));
}

TEST_CASE("AudioReaderSource discards audio decoded across a newer seek", "[audio-source][threading]") {
    LevelReader reader;
    reader.block = true;
    openshot::AudioReaderSource source(&reader, 1);
    juce::AudioBuffer<float> buffer(1, 4);
    buffer.clear();
    auto callback = std::async(std::launch::async, [&] { source.getNextAudioBlock({&buffer, 0, 4}); });
    bool entered;
    {
        std::unique_lock<std::mutex> lock(reader.mutex);
        entered = reader.condition.wait_for(lock, std::chrono::seconds(2), [&] { return reader.entered; });
    }
    if (!entered) {
        reader.Release();
        callback.get();
        REQUIRE(entered);
        return;
    }
    auto seek = std::async(std::launch::async, [&] { source.Seek(2); source.Seek(3); });
    const bool seek_completed = seek.wait_for(std::chrono::milliseconds(100)) == std::future_status::ready;
    reader.Release();
    callback.get();
    seek.get();
    CHECK(seek_completed);
    CHECK(buffer.getSample(0, 0) == Approx(0.0f));
    source.getNextAudioBlock({&buffer, 0, 4});
    CHECK(buffer.getSample(0, 0) == Approx(-0.5f));
    REQUIRE(source.getFrame());
    CHECK(source.getFrame()->number == 3);
}

TEST_CASE("AudioReaderSource pause discards a whole multi-frame block without consuming it", "[audio-source][threading]") {
    juce::AudioBuffer<float> buffer(1, 12);
    // A decoder barrier on the second frame allows the first eight samples to
    // be copied before pause invalidates the entire callback.
    class SecondFrameReader : public LevelReader {
    public:
        std::shared_ptr<openshot::Frame> GetFrame(int64_t number) override {
            block = number == 2;
            return LevelReader::GetFrame(number);
        }
    } slow;
    openshot::AudioReaderSource paused(&slow, 1);
    auto callback = std::async(std::launch::async, [&] { paused.getNextAudioBlock({&buffer, 0, 12}); });
    bool entered;
    {
        std::unique_lock<std::mutex> lock(slow.mutex);
        entered = slow.condition.wait_for(lock, std::chrono::seconds(2), [&] { return slow.entered; });
    }
    paused.setSpeed(0);
    {
        std::lock_guard<std::mutex> lock(slow.mutex);
        slow.released = true;
        slow.condition.notify_all();
    }
    callback.get();
    REQUIRE(entered);
    for (int sample = 0; sample < 12; ++sample)
        CHECK(buffer.getSample(0, sample) == Approx(0.0f));
    paused.setSpeed(1);
    paused.getNextAudioBlock({&buffer, 0, 12});
    for (int sample = 0; sample < 12; ++sample)
        CHECK(buffer.getSample(0, sample) == Approx(sample < 8 ? 0.25f : -0.5f));
    // Repeated speed assignment must not invalidate otherwise valid output.
    paused.setSpeed(1);
    paused.getNextAudioBlock({&buffer, 0, 4});
    CHECK(buffer.getSample(0, 0) == Approx(-0.5f));
}

TEST_CASE("cached Timeline audio miss holds shared gate until player resync", "[audio-source][preroll]") {
    class CacheGate : public openshot::VideoCacheThread {
    public:
        void readyAt(int64_t n) {
            std::lock_guard<std::mutex> lock(seek_state_mutex);
            cache_audio_only.store(true);
            processed_generation.store(request_generation.load());
            readiness_capacity = 20;
            readiness_physical_capacity = 21;
            readiness_minimum_step = 1;
            min_frames_ahead.store(1);
            requested_display_frame.store(n);
            updateReadiness(reader->GetCache(), n, 1);
        }
    } gate;
    class CachedTimeline : public openshot::Timeline {
    public:
        int calls = 0;
        CachedTimeline() : Timeline(16,16,openshot::Fraction(30,1),48000,1,openshot::LAYOUT_MONO) {}
        std::shared_ptr<openshot::Frame> GetFrame(int64_t) override { ++calls; return {}; }
    } timeline;
    gate.Reader(&timeline);
    gate.setSpeed(1);
    auto add = [&](int64_t n, float level) {
        auto frame = std::make_shared<openshot::Frame>(n, 8, 1);
        float samples[8];
        std::fill(samples, samples + 8, level);
        frame->AddAudio(true, 0, 0, samples, 8, 1.0f);
        timeline.GetCache()->Add(frame);
    };
    add(1, 0.1f); add(2, 0.2f); // Startup has current + minimal next frame.
    gate.readyAt(1);
    openshot::AudioReaderSource source(&timeline, 1);
    source.setVideoCache(&gate);
    juce::AudioBuffer<float> buffer(1,24); // Spans a third frame not cached yet.
    source.getNextAudioBlock({&buffer,0,24});
    CHECK(buffer.getSample(0,0) == Approx(0.1f));
    CHECK(buffer.getSample(0,8) == Approx(0.2f));
    CHECK(buffer.getSample(0,16) == Approx(0.0f));
    CHECK(timeline.calls == 0); // No synchronous Timeline decoding.
    CHECK_FALSE(gate.isReady());
    add(3, 0.3f); add(4, 0.4f);
    gate.readyAt(3);
    CHECK_FALSE(gate.isReady()); // Filling cache cannot bypass the handshake.
    source.Seek(3); // Same operation PlayerPrivate performs during shared hold.
    gate.AcknowledgeAudioCacheMiss();
    CHECK(gate.isReady());
    source.getNextAudioBlock({&buffer,0,8});
    CHECK(buffer.getSample(0,0) == Approx(0.3f));
    REQUIRE(source.getFrame());
    CHECK(source.getFrame()->number == 3);
    CHECK(timeline.calls == 0);
}
