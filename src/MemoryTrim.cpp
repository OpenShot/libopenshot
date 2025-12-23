/**
 * @file
 * @brief Cross-platform helper to encourage returning freed memory to the OS
 *
 * @ref License
 */

// Copyright (c) 2008-2025 OpenShot Studios, LLC
//
// SPDX-License-Identifier: LGPL-3.0-or-later

#include "MemoryTrim.h"

#include <atomic>
#include <chrono>
#include <cstdint>

#if defined(__GLIBC__)
#include <malloc.h>
#elif defined(_WIN32)
#include <malloc.h>
#elif defined(__APPLE__)
#include <malloc/malloc.h>
#endif

namespace {
// Limit trim attempts to once per interval to avoid spamming platform calls
constexpr uint64_t kMinTrimIntervalMs = 1000; // 1s debounce
std::atomic<uint64_t> g_last_trim_ms{0};
std::atomic<bool> g_trim_in_progress{false};

uint64_t NowMs() {
	using namespace std::chrono;
	return duration_cast<milliseconds>(steady_clock::now().time_since_epoch()).count();
}
}  // namespace

namespace openshot {

bool TrimMemoryToOS(bool force) noexcept {
	const uint64_t now_ms = NowMs();
	const uint64_t last_ms = g_last_trim_ms.load(std::memory_order_relaxed);

	// Skip if we recently trimmed (unless forced)
	if (!force && now_ms - last_ms < kMinTrimIntervalMs)
		return false;

	// Only one trim attempt runs at a time
	bool expected = false;
	if (!g_trim_in_progress.compare_exchange_strong(expected, true, std::memory_order_acq_rel))
		return false;

	bool did_trim = false;

#if defined(__GLIBC__)
	// GLIBC exposes malloc_trim to release free arenas back to the OS
	malloc_trim(0);
	did_trim = true;
#elif defined(_WIN32)
	// MinGW/MSYS2 expose _heapmin to compact the CRT heap
	_heapmin();
	did_trim = true;
#elif defined(__APPLE__)
	// macOS uses the malloc zone API to relieve memory pressure
	malloc_zone_t* zone = malloc_default_zone();
	malloc_zone_pressure_relief(zone, 0);
	did_trim = true;
#else
	// Platforms without a known trimming API
	did_trim = false;
#endif

	if (did_trim)
		g_last_trim_ms.store(now_ms, std::memory_order_relaxed);

	g_trim_in_progress.store(false, std::memory_order_release);
	return did_trim;
}

}  // namespace openshot
