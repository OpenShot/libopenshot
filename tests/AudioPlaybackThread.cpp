/**
 * @file
 * @brief Unit tests for audio playback source lifetime
 * @author OpenShot Studios, LLC
 *
 * @ref License
 */

// Copyright (c) 2026 OpenShot Studios, LLC
//
// SPDX-License-Identifier: LGPL-3.0-or-later
#include "openshot_catch.h"
#include "Qt/AudioPlaybackThread.h"
#include "DummyReader.h"
#include "QtPlayer.h"
#include "Frame.h"
#include <atomic>
#include <chrono>
#include <thread>

namespace openshot {
// Exercise the real native transport worker with its device closed. A silent
// callback pump acknowledges JUCE stop requests without a sound card or timeout.
struct AudioPlaybackThreadTestAccess {
    class SilentCallbacks {
        juce::AudioTransportSource& transport;
        std::atomic<bool> running{true};
        std::thread worker;
    public:
        explicit SilentCallbacks(juce::AudioTransportSource& source) : transport(source) {
            transport.prepareToPlay(256, 48000);
            worker = std::thread([this] {
                juce::AudioBuffer<float> buffer(2, 256);
                juce::AudioSourceChannelInfo block(&buffer, 0, 256);
                while (running.load()) {
                    transport.getNextAudioBlock(block);
                    std::this_thread::sleep_for(std::chrono::milliseconds(1));
                }
            });
        }
        ~SilentCallbacks() {
            running.store(false);
            worker.join();
            transport.releaseResources();
        }
    };
    class SilentRenderer : public RendererBase {
        void render(std::shared_ptr<QImage>) override {}
    public:
        void OverrideWidget(uintptr_t) override {}
    };
    static void checkPlayerRestart() {
        AudioDeviceManagerSingleton::Instance()->audioDeviceManager.closeAudioDevice();
        SilentRenderer renderer;
        DummyReader reader(Fraction(30, 1), 16, 16, 48000, 1, 2);
        reader.info.has_audio = true;
        reader.Open();
        QtPlayer player(&renderer);
        player.Reader(&reader);
        auto* audio = player.p->audioPlayback;
        auto* original = audio->source;
        SilentCallbacks callbacks(audio->transport);
        for (int iteration = 0; iteration < 3; ++iteration) {
            player.Play();
            player.Play();
            auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds(2);
            while (!audio->transport.isPlaying() && std::chrono::steady_clock::now() < deadline)
                std::this_thread::sleep_for(std::chrono::milliseconds(1));
            const bool started = audio->transport.isPlaying();
            player.Stop();
            REQUIRE(started);
            CHECK(audio->source == original);
            CHECK_FALSE(audio->is_playing.load());
            CHECK_FALSE(audio->isThreadRunning());
        }
    }
    static void checkRestart() {
        AudioDeviceManagerSingleton::Instance()->audioDeviceManager.closeAudioDevice();
        DummyReader reader(Fraction(30, 1), 16, 16, 48000, 1, 2);
        reader.Open();
        AudioPlaybackThread audio(nullptr);
        audio.Reader(&reader);
        const AudioReaderSource* original = audio.source;
        SilentCallbacks callbacks(audio.transport);
        for (int iteration = 0; iteration < 3; ++iteration) {
            audio.Play();
            audio.startThread();
            auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds(2);
            while (!audio.transport.isPlaying() && std::chrono::steady_clock::now() < deadline)
                std::this_thread::sleep_for(std::chrono::milliseconds(1));
            const bool started = audio.transport.isPlaying();
            audio.Stop();
            audio.stopThread(-1);
            REQUIRE(started);
            CHECK(audio.source == original);
            CHECK_FALSE(audio.is_playing.load());
            CHECK_FALSE(audio.transport.isPlaying());
        }
        // Reader replacement after joining uses the retained source safely.
        DummyReader replacement(Fraction(30, 1), 16, 16, 44100, 2, 2);
        replacement.Open();
        audio.Reader(&replacement);
        CHECK(audio.source == original);
        CHECK(audio.source->Reader() == &replacement);
        CHECK(audio.is_playing.load());
        audio.Stop();
    }
};
}

TEST_CASE("Audio playback retains its source across transport Stop and Play", "[audio-transport][threading]") {
    openshot::AudioPlaybackThreadTestAccess::checkRestart();
}

TEST_CASE("QtPlayer public Stop and Play restart the retained audio transport", "[audio-transport][qtplayer][threading]") {
    openshot::AudioPlaybackThreadTestAccess::checkPlayerRestart();
}
