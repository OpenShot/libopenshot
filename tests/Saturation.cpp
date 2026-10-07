/**
 * @file
 * @brief Unit tests for Saturation effect behavior
 */

// Copyright (c) 2008-2026 OpenShot Studios, LLC
//
// SPDX-License-Identifier: LGPL-3.0-or-later

#include "openshot_catch.h"

#include <vector>

#include <QImage>

#include "Frame.h"
#include "effects/Saturation.h"

using namespace openshot;

namespace {
	void fill_rgba_pattern(QImage& image) {
		for (int y = 0; y < image.height(); ++y) {
			auto* row = image.scanLine(y);
			for (int x = 0; x < image.width(); ++x) {
				const int idx = x * 4;
				row[idx + 0] = static_cast<unsigned char>((x * 17 + y * 3) % 256);
				row[idx + 1] = static_cast<unsigned char>((x * 5 + y * 11) % 256);
				row[idx + 2] = static_cast<unsigned char>((x * 13 + y * 7) % 256);
				row[idx + 3] = 255;
			}
		}
	}
}

TEST_CASE("Saturation respects padded QImage stride in OpenMP path", "[libopenshot][effect][saturation][omp]")
{
	const int width = 13;
	const int height = 1400;
	const int padded_stride = (width + 5) * 4;
	const unsigned char sentinel = 0xA5;

	std::vector<unsigned char> padded_buffer(padded_stride * height, sentinel);
	auto padded = std::make_shared<QImage>(
		padded_buffer.data(), width, height, padded_stride,
		QImage::Format_RGBA8888_Premultiplied);
	fill_rgba_pattern(*padded);

	QImage tight(width, height, QImage::Format_RGBA8888_Premultiplied);
	fill_rgba_pattern(tight);

	auto padded_frame = std::make_shared<Frame>(1, width, height, "#000000", 0, 2);
	padded_frame->AddImage(padded);
	auto tight_frame = std::make_shared<Frame>(1, width, height, "#000000", 0, 2);
	tight_frame->AddImage(std::make_shared<QImage>(tight));

	Saturation padded_effect(Keyframe(0.35), Keyframe(1.1), Keyframe(0.9), Keyframe(1.2));
	Saturation tight_effect(Keyframe(0.35), Keyframe(1.1), Keyframe(0.9), Keyframe(1.2));

	auto padded_out = padded_effect.GetFrame(padded_frame, 1)->GetImage();
	auto tight_out = tight_effect.GetFrame(tight_frame, 1)->GetImage();

	REQUIRE(padded_out->bytesPerLine() == padded_stride);
	bool active_pixels_match = true;
	bool padding_unchanged = true;
	for (int y = 0; y < height; ++y) {
		const auto* padded_row = padded_out->constScanLine(y);
		const auto* tight_row = tight_out->constScanLine(y);
		for (int x = 0; x < width * 4; ++x) {
			active_pixels_match = active_pixels_match && padded_row[x] == tight_row[x];
		}
		for (int x = width * 4; x < padded_stride; ++x) {
			padding_unchanged = padding_unchanged && padded_row[x] == sentinel;
		}
	}
	CHECK(active_pixels_match);
	CHECK(padding_unchanged);
}
