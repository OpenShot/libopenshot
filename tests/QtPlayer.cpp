/**
 * @file
 * @brief Unit tests for openshot::QtPlayer
 * @author OpenShot Studios, LLC
 *
 * @ref License
 */

// Copyright (c) 2008-2025 OpenShot Studios, LLC
//
// SPDX-License-Identifier: LGPL-3.0-or-later

#include "openshot_catch.h"

#include <memory>
#include <atomic>
#include <chrono>
#include <thread>
#include <future>

#include "DummyReader.h"
#include "QtPlayer.h"
#include "Qt/VideoRenderer.h"

class QWidget;

namespace {
class TestRenderer : public openshot::RendererBase
{
public:
	uintptr_t last_widget = 0;

	void OverrideWidget(uintptr_t qwidget_address) override
	{
		last_widget = qwidget_address;
	}

protected:
	void render(std::shared_ptr<QImage> image) override
	{
		(void) image;
	}
};

class SlowRenderer : public openshot::RendererBase
{
public:
	std::atomic<int> renders{0};
	void OverrideWidget(uintptr_t) override {}

protected:
	void render(std::shared_ptr<QImage> image) override
	{
		if (image)
			++renders;
		// Longer than the player's render timeout: the next frame may be
		// submitted while this renderer still owns the previous one.
		std::this_thread::sleep_for(std::chrono::milliseconds(120));
	}
};

class SlowReader : public openshot::DummyReader
{
public:
	std::atomic<bool> decoding{false};
	SlowReader() : DummyReader(openshot::Fraction(30, 1), 16, 16, 44100, 2, 30) {}

	std::shared_ptr<openshot::Frame> GetFrame(int64_t frame_number) override
	{
		decoding = true;
		std::this_thread::sleep_for(std::chrono::milliseconds(250));
		auto frame = DummyReader::GetFrame(frame_number);
		decoding = false;
		return frame;
	}
};
} // namespace

TEST_CASE("QtPlayer_GetRendererQObject_ReturnsVideoRendererAddress", "[libopenshot][qtplayer]")
{
	auto renderer = std::make_unique<VideoRenderer>();
	openshot::QtPlayer player(renderer.get());

	auto addr = player.GetRendererQObject();
	CHECK(addr == reinterpret_cast<uintptr_t>(renderer.get()));
}

TEST_CASE("QtPlayer_SetQWidget_Overload_ForwardsPointer", "[libopenshot][qtplayer]")
{
	TestRenderer renderer;
	openshot::QtPlayer player(&renderer);

	char dummy = 0;
	auto *widget = reinterpret_cast<QWidget*>(&dummy);
	player.SetQWidget(widget);

	CHECK(renderer.last_widget == reinterpret_cast<uintptr_t>(widget));
}

TEST_CASE("QtPlayer keeps frames alive during slow rendering and seeks", "[libopenshot][qtplayer][threading]")
{
	SlowRenderer renderer;
	openshot::DummyReader reader(openshot::Fraction(30, 1), 16, 16, 44100, 2, 30);
	reader.Open();
	openshot::QtPlayer player(&renderer);
	player.Reader(&reader);
	player.Play();

	std::thread seeker([&player] {
		for (int i = 0; i < 80; ++i) {
			player.Seek(1 + (i % 60), false);
			std::this_thread::sleep_for(std::chrono::milliseconds(2));
		}
	});
	seeker.join();
	std::this_thread::sleep_for(std::chrono::milliseconds(350));
	player.Stop();
	CHECK(renderer.renders.load() > 0);
}

TEST_CASE("QtPlayer seek does not wait for a slow frame decode", "[libopenshot][qtplayer][threading]")
{
	TestRenderer renderer;
	SlowReader reader;
	reader.Open();
	openshot::QtPlayer player(&renderer);
	player.Reader(&reader);
	player.Play();
	const auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds(2);
	while (!reader.decoding && std::chrono::steady_clock::now() < deadline)
		std::this_thread::sleep_for(std::chrono::milliseconds(1));
	REQUIRE(reader.decoding.load());
	const auto start = std::chrono::steady_clock::now();
	player.Seek(20, false);
	const auto elapsed = std::chrono::steady_clock::now() - start;
	CHECK(elapsed < std::chrono::milliseconds(100));
	player.Stop();
}

namespace {
class HandoffReader : public openshot::DummyReader {
public:
    HandoffReader() : DummyReader(openshot::Fraction(30, 1), 16, 16, 44100, 2, 30) {
        info.has_audio = false; // Test the video/cache ownership independently of device access.
    }
    std::promise<void> entered;
    std::promise<void> release;
    std::shared_future<void> released = release.get_future().share();
    std::atomic<int> calls{0};
    std::shared_ptr<openshot::Frame> GetFrame(int64_t number) override {
        if (calls.fetch_add(1) == 0) {
            entered.set_value();
            released.wait();
        }
        return DummyReader::GetFrame(number);
    }
};
}

TEST_CASE("QtPlayer reader handoff drains pending work in playing and paused modes", "[libopenshot][qtplayer][handoff]") {
    const bool paused = GENERATE(false, true);
    TestRenderer renderer;
    auto old_reader = std::make_unique<HandoffReader>();
    HandoffReader current_reader;
    current_reader.release.set_value();
    old_reader->Open();
    current_reader.Open();
    openshot::QtPlayer player(&renderer);
    player.Reader(old_reader.get());
    auto entered = old_reader->entered.get_future();
    auto current_entered = current_reader.entered.get_future();
    player.Play();
    const bool decoding = entered.wait_for(std::chrono::seconds(2)) == std::future_status::ready;
    if (!decoding) {
        old_reader->release.set_value();
        player.Stop();
    }
    REQUIRE(decoding);
    if (paused)
        player.Pause();
    auto handoff = std::async(std::launch::async, [&] { player.Reader(&current_reader); });
    CHECK(handoff.wait_for(std::chrono::milliseconds(20)) == std::future_status::timeout);
    old_reader->release.set_value();
    handoff.get();
    old_reader.reset(); // Caller may immediately delete the old raw reader.
    const bool current_decoding = current_entered.wait_for(std::chrono::seconds(2)) == std::future_status::ready;
    player.Stop();
    CHECK(current_decoding); // Includes paused replacement at the same position.
}

TEST_CASE("QtPlayer destruction drains an active decoder", "[libopenshot][qtplayer][handoff]") {
    TestRenderer renderer;
    HandoffReader reader;
    reader.Open();
    auto player = std::make_unique<openshot::QtPlayer>(&renderer);
    player->Reader(&reader);
    auto entered = reader.entered.get_future();
    player->Play();
    const bool decoding = entered.wait_for(std::chrono::seconds(2)) == std::future_status::ready;
    if (!decoding) {
        reader.release.set_value();
        player->Stop();
    }
    REQUIRE(decoding);
    auto destroy = std::async(std::launch::async, [owned = std::move(player)]() mutable { owned.reset(); });
    CHECK(destroy.wait_for(std::chrono::milliseconds(20)) == std::future_status::timeout);
    reader.release.set_value();
    destroy.get();
    CHECK(reader.calls.load() == 1);
}
