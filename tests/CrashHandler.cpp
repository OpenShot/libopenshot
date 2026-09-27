// Copyright (c) 2026 OpenShot Studios, LLC
// SPDX-License-Identifier: LGPL-3.0-or-later
#include "openshot_catch.h"
#include "CrashHandler.h"

#ifndef _WIN32
#include <cerrno>
#include <csignal>
#include <unistd.h>

TEST_CASE("Crash handler preserves ignored SIGPIPE", "[crash-handler]") {
    // Python ignores SIGPIPE so a disconnected console raises EPIPE instead
    // of terminating the editor. Native initialization must preserve that.
    struct sigaction ignored = {};
    ignored.sa_handler = SIG_IGN;
    sigemptyset(&ignored.sa_mask);
    REQUIRE(sigaction(SIGPIPE, &ignored, nullptr) == 0);
    openshot::CrashHandler::Instance();
    struct sigaction current = {};
    REQUIRE(sigaction(SIGPIPE, nullptr, &current) == 0);
    REQUIRE(current.sa_handler == SIG_IGN);

    int pipe_fds[2];
    REQUIRE(pipe(pipe_fds) == 0);
    close(pipe_fds[0]);
    const auto result = write(pipe_fds[1], "x", 1);
    const auto error = errno;
    close(pipe_fds[1]);
    CHECK(result == -1);
    CHECK(error == EPIPE);
}
#endif
