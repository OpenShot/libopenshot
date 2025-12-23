/**
 * @file
 * @brief Unit tests for openshot::Wave effect
 * @author OpenAI ChatGPT
 */

// Copyright (c) 2008-2025 OpenShot Studios, LLC
//
// SPDX-License-Identifier: LGPL-3.0-or-later

#include <memory>
#include <QImage>
#include <QColor>

#include "Frame.h"
#include "effects/Wave.h"
#include "openshot_catch.h"

using namespace openshot;

TEST_CASE("Wave uses original pixel buffer", "[effect][wave]")
{
	// Create 1x10 image with increasing red channel
	QImage img(10, 1, QImage::Format_ARGB32);
	for (int x = 0; x < 10; ++x)
		img.setPixelColor(x, 0, QColor(x, 0, 0, 255));
	auto f = std::make_shared<Frame>();
	*f->GetImage() = img;

	Wave w;
	w.wavelength = Keyframe(0.0);
	w.amplitude = Keyframe(1.0);
	w.multiplier = Keyframe(0.01);
	w.shift_x = Keyframe(-1.0); // negative shift to copy from previous pixel
	w.speed_y = Keyframe(0.0);

	auto out_img = w.GetFrame(f, 1)->GetImage();
	int expected[10] = {0,0,1,2,3,4,5,6,7,8};
	for (int x = 0; x < 10; ++x)
		CHECK(out_img->pixelColor(x,0).red() == expected[x]);
}
