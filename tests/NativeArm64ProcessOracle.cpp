/**
 * @file
 * @brief Unit tests for the Windows native Arm64 process/payload
 *        architecture oracle (design-spec.md G2/G3/G8/G11,
 *        design-amendment-A1).
 * @author OpenShot Studios, LLC
 *
 * @ref License
 */

// Copyright (c) 2008-2026 OpenShot Studios, LLC
//
// SPDX-License-Identifier: LGPL-3.0-or-later

#include "openshot_catch.h"
#include <iomanip>

#if defined(_WIN32)
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#endif

// This test validates design-amendment-A1's approved native-process oracle
// semantics directly against the running test host:
//   - pNativeMachine must equal IMAGE_FILE_MACHINE_ARM64 (0xAA64) for a
//     native Arm64 host.
//   - pProcessMachine must equal IMAGE_FILE_MACHINE_UNKNOWN (0x0) for a
//     process that is running natively (not under WOW64/emulation).
//   - Any nonzero pProcessMachine indicates WOW/emulated execution and is
//     reported, never silently treated as a pass.
//
// This test intentionally does NOT assert host architecture except in a
// native Arm64 build. It captures observed values for assertion diagnostics: on this
// AMD64 development/CI host it demonstrates the API and reports
// native_machine == AMD64 (not ARM64), which is expected and does not
// constitute an Arm64 release claim. Only on an actual native Arm64 host
// would native_arm64_ok become true.
TEST_CASE( "NativeArm64ProcessOracle_A1", "[libopenshot][windows][arm64]" )
{
#if defined(_WIN32)
    // IsWow64Process2 requires Windows 10 1809 (build 17763) or later.
    HMODULE kernel32 = ::GetModuleHandleW(L"kernel32.dll");
    REQUIRE(kernel32 != nullptr);

    using IsWow64Process2Fn = BOOL (WINAPI*)(HANDLE, USHORT*, USHORT*);
    auto pIsWow64Process2 = reinterpret_cast<IsWow64Process2Fn>(
        ::GetProcAddress(kernel32, "IsWow64Process2"));

    if (!pIsWow64Process2) {
        WARN("IsWow64Process2 is unavailable on this Windows build "
             "(requires 10.0.17763+); native-process oracle skipped.");
        return;
    }

    USHORT processMachine = IMAGE_FILE_MACHINE_UNKNOWN;
    USHORT nativeMachine = IMAGE_FILE_MACHINE_UNKNOWN;
    ::SetLastError(ERROR_SUCCESS);
    BOOL ok = pIsWow64Process2(::GetCurrentProcess(), &processMachine, &nativeMachine);
    const DWORD lastError = ::GetLastError();
    INFO("GetLastError=" << lastError);
    REQUIRE(ok);

    const bool isWowOrEmulated = (processMachine != IMAGE_FILE_MACHINE_UNKNOWN);
    const bool nativeArm64Ok =
        (nativeMachine == IMAGE_FILE_MACHINE_ARM64) &&
        (processMachine == IMAGE_FILE_MACHINE_UNKNOWN);

    INFO("process_machine=0x" << std::hex << processMachine);
    INFO("native_machine=0x" << std::hex << nativeMachine);
    INFO("is_wow_or_emulated=" << isWowOrEmulated);
    INFO("native_arm64_ok=" << nativeArm64Ok);
    if (isWowOrEmulated) {
        WARN("Process is running under WOW/emulation.");
    }
    // On an Arm64 host, any nonzero process machine is WOW/emulated and must
    // fail. Other hosts only prove that they are not native Arm64.
    if (nativeMachine == IMAGE_FILE_MACHINE_ARM64) {
        REQUIRE_FALSE(isWowOrEmulated);
        REQUIRE(nativeArm64Ok);
    } else {
        CHECK_FALSE(nativeArm64Ok);
    }
#else
    WARN("IsWow64Process2 is a Windows-only API; native-process oracle skipped on this platform.");
#endif
}
