#!/usr/bin/env bash
set -euo pipefail
arch=${1:?mingw64 or mingw32}
prefix=${PREFIX:-/c/runner-setup/ffmpeg-7.1.3-$arch}
export PATH="$prefix/bin:/$arch/bin:/usr/bin:/c/Windows/System32:/c/Windows"
exec > >(tee "/c/runner-setup/logs/ffmpeg-smoke-$arch.log") 2>&1
"$prefix/bin/ffmpeg.exe" -version
"$prefix/bin/ffmpeg.exe" -hide_banner -hwaccels | grep vulkan
"$prefix/bin/ffmpeg.exe" -hide_banner -encoders > "$prefix/compiled-encoders.txt" 2>&1
for codec in h264_nvenc hevc_nvenc h264_qsv hevc_qsv h264_amf hevc_amf; do
 grep -Eq "[[:space:]]$codec[[:space:]]" "$prefix/compiled-encoders.txt"
done
"$prefix/bin/ffmpeg.exe" -hide_banner -devices > "$prefix/compiled-devices.txt" 2>&1
grep -q gdigrab "$prefix/compiled-devices.txt"
grep -q dshow "$prefix/compiled-devices.txt"
"$prefix/bin/ffmpeg.exe" -hide_banner -f lavfi -i testsrc2=size=320x240:rate=24  -f lavfi -i sine=frequency=440:sample_rate=48000 -t 2  -c:v libx264 -preset fast -pix_fmt yuv420p -c:a aac -y "$prefix/h264-aac-test.mp4"
"$prefix/bin/ffmpeg.exe" -v error -i "$prefix/h264-aac-test.mp4" -f null -
"$prefix/bin/ffprobe.exe" -v error -show_entries stream=codec_name,width,height,sample_rate "$prefix/h264-aac-test.mp4"
"$prefix/bin/ffmpeg.exe" -v error -i /c/runner-setup/libopenshot-vulkan-mvp/examples/test_video_sync.mp4 -frames:v 3 -f null -
echo "SUCCESS: vendor encoders, screen/camera capture, Vulkan compiled in; H.264/AAC encode and decode $arch"
