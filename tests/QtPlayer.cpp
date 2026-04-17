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
