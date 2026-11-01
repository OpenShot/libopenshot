#!/usr/bin/env bash
set -euo pipefail
arch=${1:?Specify mingw64 or mingw32}
export PATH="/$arch/bin:/usr/bin:/c/Windows/System32:/c/Windows"
export WITH_VULKAN=${WITH_VULKAN:-1}
bash "$(dirname "$0")/setup-hw-ffmpeg-windows.sh" "$arch"
exec bash "$(dirname "$0")/setup-ffmpeg-windows-final.sh" "$@"
