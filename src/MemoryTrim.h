/**
 * @file
 * @brief Cross-platform helper to encourage returning freed memory to the OS
 *
 * @ref License
 */

// Copyright (c) 2008-2025 OpenShot Studios, LLC
//
// SPDX-License-Identifier: LGPL-3.0-or-later

#pragma once

namespace openshot {

/**
 * @brief Attempt to return unused heap memory to the operating system.
 *
 * This maps to the appropriate platform-specific API where available.
 * The call is safe to invoke on any supported platform; on unsupported
 * platforms it will simply return false without doing anything.
 * Calls are rate-limited internally (1s debounce) and single-flight. A forced
 * call bypasses the debounce but still honors the single-flight guard.
 *
 * @param force If true, bypass the debounce interval (useful for teardown).
 * @return true if a platform-specific trim call was made, false otherwise.
 */
bool TrimMemoryToOS(bool force = false) noexcept;

}  // namespace openshot
