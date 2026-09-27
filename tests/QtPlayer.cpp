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
