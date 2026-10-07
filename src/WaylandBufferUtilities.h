/**
 * @file
 * @brief Bounds-safe helpers for mapped PipeWire video buffers
 */

// Copyright (c) 2008-2026 OpenShot Studios, LLC
//
// SPDX-License-Identifier: LGPL-3.0-or-later

#ifndef OPENSHOT_WAYLAND_BUFFER_UTILITIES_H
#define OPENSHOT_WAYLAND_BUFFER_UTILITIES_H

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <limits>

namespace openshot::wayland
{
	struct PackedVideoLayout
	{
		size_t offset = 0;
		size_t valid_size = 0;
		int stride = 0;
		int crop_x = 0;
		int crop_y = 0;
		int width = 0;
		int height = 0;
		bool valid = false;
	};

	inline PackedVideoLayout ResolvePackedVideoLayout(
		size_t max_size,
		size_t chunk_offset,
		size_t chunk_size,
		int chunk_stride,
		int stream_width,
		int stream_height,
		int crop_x,
		int crop_y,
		int crop_width,
		int crop_height)
	{
		PackedVideoLayout layout;
		if (max_size == 0 || stream_width <= 0 || stream_height <= 0
				|| stream_width > std::numeric_limits<int>::max() / 4
				|| chunk_stride < 0) {
			return layout;
		}

		layout.offset = chunk_offset % max_size;
		layout.valid_size = std::min(chunk_size, max_size);
		layout.stride = chunk_stride > 0 ? chunk_stride : stream_width * 4;
		if (layout.stride < 4 || layout.valid_size < static_cast<size_t>(layout.stride)) {
			return layout;
		}

		const int readable_width = std::min(
			stream_width,
			layout.stride / 4);
		const int readable_height = std::min(
			stream_height,
			static_cast<int>(layout.valid_size / static_cast<size_t>(layout.stride)));

		layout.crop_x = std::max(0, crop_x);
		layout.crop_y = std::max(0, crop_y);
		if (layout.crop_x >= readable_width || layout.crop_y >= readable_height) {
			return layout;
		}

		const int requested_width = crop_width > 0
			? crop_width
			: stream_width - layout.crop_x;
		const int requested_height = crop_height > 0
			? crop_height
			: stream_height - layout.crop_y;
		layout.width = std::min(requested_width, readable_width - layout.crop_x);
		layout.height = std::min(requested_height, readable_height - layout.crop_y);

		// H.264 requires even dimensions, and capture callers already expect them.
		layout.width -= layout.width % 2;
		layout.height -= layout.height % 2;
		if (layout.width <= 0 || layout.height <= 0) {
			return layout;
		}

		const size_t final_row_end =
			static_cast<size_t>(layout.crop_y + layout.height - 1) * layout.stride
			+ static_cast<size_t>(layout.crop_x + layout.width) * 4;
		if (final_row_end > layout.valid_size) {
			return layout;
		}

		layout.valid = true;
		return layout;
	}

	inline int DamageFrameWaitMilliseconds(int fps_num, int fps_den, bool have_last_frame)
	{
		if (!have_last_frame) {
			return 5000;
		}
		const double fps = fps_num > 0 && fps_den > 0
			? static_cast<double>(fps_num) / fps_den
			: 30.0;
		return std::max(1, static_cast<int>(1000.0 / std::max(1.0, fps)));
	}

	inline bool CopyWrappedBytes(
		const uint8_t* source,
		size_t max_size,
		size_t chunk_offset,
		size_t logical_offset,
		uint8_t* destination,
		size_t byte_count)
	{
		if (!source || !destination || max_size == 0 || byte_count > max_size) {
			return false;
		}
		const size_t physical_offset = (
			(chunk_offset % max_size) + (logical_offset % max_size)
		) % max_size;
		const size_t first_size = std::min(byte_count, max_size - physical_offset);
		std::memcpy(destination, source + physical_offset, first_size);
		if (first_size < byte_count) {
			std::memcpy(destination + first_size, source, byte_count - first_size);
		}
		return true;
	}
}

#endif
