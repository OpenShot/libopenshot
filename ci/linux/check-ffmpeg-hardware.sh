#!/usr/bin/env bash
# Compiled support checks are GPU-independent. Real driver/encoding tests are separate.
set -euo pipefail
root=${1:?FFmpeg installation prefix}
log=${2:-$root/hardware-validation}
mkdir -p "$log"
export LD_LIBRARY_PATH="$root/lib:${LD_LIBRARY_PATH:-}"
"$root/bin/ffmpeg" -hide_banner -version > "$log/version.txt"
"$root/bin/ffmpeg" -hide_banner -encoders > "$log/encoders.txt" 2>&1
"$root/bin/ffmpeg" -hide_banner -hwaccels > "$log/hwaccels.txt" 2>&1
for codec in h264_nvenc hevc_nvenc h264_qsv hevc_qsv h264_amf hevc_amf h264_vaapi hevc_vaapi; do
 grep -Eq "[[:space:]]$codec[[:space:]]" "$log/encoders.txt" || { echo "Missing required encoder: $codec"; exit 1; }
done
ldd "$root/lib/libavcodec.so.61" > "$log/avcodec-dependencies.txt"
if grep -q 'not found' "$log/avcodec-dependencies.txt"; then cat "$log/avcodec-dependencies.txt"; exit 1; fi
grep -q 'libvpl.so.2' "$log/avcodec-dependencies.txt"
"$root/bin/ffmpeg" -v error -f lavfi -i testsrc2=size=320x240:rate=24 -f lavfi -i sine=sample_rate=48000 -t 2 -c:v libx264 -threads 2 -c:a aac -y "$log/h264-aac.mp4"
"$root/bin/ffmpeg" -v error -i "$log/h264-aac.mp4" -f null -
"$root/bin/ffmpeg" -v error -f lavfi -i testsrc2=size=128x96:rate=4 -frames:v 4 -c:v libaom-av1 -cpu-used 8 -threads 2 -y "$log/av1.webm"
"$root/bin/ffmpeg" -v error -c:v libdav1d -i "$log/av1.webm" -f null -
echo 'SUCCESS: NVENC, QSV, AMF and VA-API encoders compiled; dispatcher linked; H.264/AAC and AV1 software smoke tests passed'
