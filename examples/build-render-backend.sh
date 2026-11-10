#!/usr/bin/env bash
# Build isolated CPU/Vulkan experiments without replacing installed libraries.
set -euo pipefail

backend=${OPENSHOT_RENDER_BACKEND:-cpu}
case "$backend" in
    cpu) vulkan=OFF; target=openshot-benchmark ;;
    vulkan) vulkan=ON; target=openshot-vulkan-benchmark ;;
    *) echo 'OPENSHOT_RENDER_BACKEND must be cpu or vulkan' >&2; exit 2 ;;
esac

source_dir=$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")/.." && pwd)
build_dir=${OPENSHOT_BUILD_DIR:-"$source_dir/build-mvp-$backend"}

# Additional arguments are CMake dependency/toolchain settings. Backend and
# Release settings below take precedence so the two experiments stay comparable.
cmake -S "$source_dir" -B "$build_dir" "$@" \
    -DCMAKE_BUILD_TYPE=Release \
    -DENABLE_VULKAN_BENCHMARK="$vulkan" \
    -DENABLE_GPU_FRAME="$vulkan" \
    -DENABLE_PYTHON=ON \
    -DBUILD_TESTING=ON
cmake --build "$build_dir" --target "$target" openshot-timeline-benchmark pyopenshot --parallel "${OPENSHOT_BUILD_JOBS:-4}"

if [[ "$backend" == vulkan ]]; then
    echo "Built experimental Vulkan benchmark: $build_dir/examples/$target"
    echo 'Experimental GPU library enabled; verify GPU residency and fallbacks with the timeline benchmark.'
else
    echo "Built CPU library and benchmark: $build_dir/tests/$target"
fi
