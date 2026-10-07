#!/usr/bin/env bash
# Copyright (c) 2026 OpenShot Studios, LLC
# SPDX-License-Identifier: LGPL-3.0-or-later

# Run after building the FFmpegReader and FFmpegWriter test targets.
# Usage: bash tests/check-stability.sh [build-directory]
set -euo pipefail
build_dir="${1:-build}"
export QT_QPA_PLATFORM=offscreen

# Fail on invalid memory access/free and definite reader leaks. The timeout
# also turns a lifecycle deadlock into a failure instead of hanging CI.
timeout 120s valgrind --error-exitcode=99 --leak-check=full \
    --errors-for-leak-kinds=definite \
    "$build_dir/tests/openshot-FFmpegReader-test" '[lifecycle]'

# Writer cleanup has known pre-existing leaks. Check invalid reads/writes and
# double frees here without suppressions that might hide the ownership bug.
# This command does not certify leak-free export.
timeout 120s valgrind --error-exitcode=99 --leak-check=no \
    "$build_dir/tests/openshot-FFmpegWriter-test" '[rawvideo]'
