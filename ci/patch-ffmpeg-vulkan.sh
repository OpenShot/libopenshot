#!/usr/bin/env bash
# Apply the FFmpeg 7.1 Vulkan host-import and staging-copy fixes.
set -euo pipefail
script_dir=$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")" && pwd)
source_dir=${1:?Specify the FFmpeg source directory}
fix="$script_dir/patches/ffmpeg-vulkan-readback.patch"
if patch --dry-run --reverse --silent -d "$source_dir" -p1 < "$fix" >/dev/null 2>&1; then
    echo "FFmpeg Vulkan readback fixes already applied"
else
    patch --forward -d "$source_dir" -p1 < "$fix"
fi
