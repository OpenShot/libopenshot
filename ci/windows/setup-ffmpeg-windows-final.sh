#!/usr/bin/env bash
# Run using C:\msys64\usr\bin\bash.exe. No pacman changes.
# Usage: bash setup-ffmpeg-windows.sh mingw64|mingw32
set -euo pipefail
arch=${1:?Specify mingw64 or mingw32}
case "$arch" in mingw64) cpu=x86_64 ;; mingw32) cpu=x86 ;; *) exit 2 ;; esac
version=7.1.3
root=/c/runner-setup
prefix=${PREFIX:-$root/ffmpeg-$version-$arch}
hw_prefix=${HW_PREFIX:-$root/hw-ffmpeg-$arch}
work=${BUILD_DIR:-$root/build-$version-$arch}
export PATH="$root/tools/bin:/$arch/bin:/usr/bin:/c/Windows/System32:/c/Windows"
export PKG_CONFIG_LIBDIR="/$arch/lib/pkgconfig:/$arch/share/pkgconfig"
export PKG_CONFIG_PATH="$hw_prefix/lib/pkgconfig:$prefix/lib/pkgconfig"
mkdir -p "$root/logs" "$prefix" "$work"
exec > >(tee -a "$root/logs/ffmpeg-$version-$arch.log") 2>&1
trap 'echo "FAILED at line $LINENO"; exit 1' ERR
echo "Building $version $arch at $(date)"
gcc --version | head -1
cd "$root"
if [ ! -f "FFmpeg-n$version.tar.gz" ]; then
 curl --cacert /usr/ssl/cert.pem -fL --retry 2 "https://github.com/FFmpeg/FFmpeg/archive/refs/tags/n$version.tar.gz" -o "FFmpeg-n$version.tar.gz"
fi
printf '%s  %s\n' e0b04c4b43d7e6d67cb6710334fb513adf13ac860532f30e1d0ac4c231f232fb "FFmpeg-n$version.tar.gz" | sha256sum -c -
if [ ! -d "$root/FFmpeg-n$version" ]; then tar xzf "FFmpeg-n$version.tar.gz"; fi
cd "$work"
opts=(--prefix="$prefix" --target-os=mingw32 --arch="$cpu"
 --enable-gpl --enable-version3 --enable-shared --disable-static --disable-doc
 --enable-ffnvcodec --enable-nvenc --enable-nvdec --enable-cuvid --enable-amf --enable-libvpl
 --extra-cflags="-I$hw_prefix/include"
 --enable-x86asm --x86asmexe="$root/tools/bin/nasm.exe" --enable-libdav1d --extra-ldflags="-L$prefix/lib")
if [ "${WITH_VULKAN:-0}" = 1 ]; then
 cflags="-I$hw_prefix/include -I$root/vulkan-$arch/include"
 if [ "$arch" = mingw32 ]; then cflags="$cflags -msse2 -mfpmath=sse"; fi
 opts+=(--enable-vulkan --extra-cflags="$cflags")
else
 opts+=(--disable-vulkan)
fi
# Enable the existing codec libraries explicitly; configure reports incompatible ones.
for pair in libx264:x264 libx265:x265 libvpx:vpx libopus:opus libvorbis:vorbis libtheora:theoraenc libmp3lame:lame libwebp:libwebp libopenjpeg:libopenjp2 libass:libass libfreetype:freetype2 libfontconfig:fontconfig libfribidi:fribidi libsoxr:soxr; do
 feature=${pair%%:*}; package=${pair#*:}
 if pkg-config --exists "$package"; then opts+=("--enable-$feature"); fi
done
"$root/FFmpeg-n$version/configure" "${opts[@]}"
make -j"${JOBS:-6}"
make install
# The oneVPL dispatcher is an application dependency; vendor GPU drivers remain system supplied.
cp "$hw_prefix/bin/libvpl.dll" "$prefix/bin/"
mkdir -p "$prefix/share/vpl/licensing"
cp "$hw_prefix/share/vpl/licensing/"* "$prefix/share/vpl/licensing/"
export PATH="$prefix/bin:$hw_prefix/bin:$PATH"
"$prefix/bin/ffmpeg.exe" -version
"$prefix/bin/ffmpeg.exe" -hide_banner -f lavfi -i testsrc2=size=128x96:rate=5 -t 1 -c:v mpeg4 -y "$prefix/smoke-test.avi"
"$prefix/bin/ffprobe.exe" -v error -show_entries stream=codec_name,width,height "$prefix/smoke-test.avi"
cat > "$prefix/env.sh" <<ENV
export PATH=$prefix/bin:$hw_prefix/bin:/$arch/bin:/usr/bin:\$PATH
export PKG_CONFIG_PATH=$prefix/lib/pkgconfig
export FFMPEG_ROOT=$prefix
ENV
"$prefix/bin/ffmpeg.exe" -hide_banner -encoders > "$prefix/compiled-encoders.txt" 2>&1
for codec in h264_nvenc hevc_nvenc h264_qsv hevc_qsv h264_amf hevc_amf; do
 grep -Eq "[[:space:]]$codec[[:space:]]" "$prefix/compiled-encoders.txt" || { echo "Missing required encoder: $codec"; exit 1; }
done
cp config.h config_components.h "$prefix/"
echo "SUCCESS: $prefix includes NVIDIA NVENC, Intel QSV, and AMD AMF"
