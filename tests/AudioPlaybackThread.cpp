// SPDX-License-Identifier: LGPL-3.0-or-later
#include "openshot_catch.h"
#include "Qt/AudioPlaybackThread.h"
#include "DummyReader.h"
#include "QtPlayer.h"
#include "Frame.h"
#include <chrono>
#include <thread>

namespace openshot {
// Exercise the real native transport worker with its device closed. This seam
// adds no runtime API and does not need a sound card or emit audio.
struct AudioPlaybackThreadTestAccess {
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
        AudioReaderSource* original = audio.source;
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
