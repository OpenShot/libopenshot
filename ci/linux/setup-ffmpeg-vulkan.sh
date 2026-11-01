#!/usr/bin/env bash
# Install FFmpeg with Vulkan (plus the codecs the focal AppImage ships) into
# /opt, without touching the system FFmpeg used by the CPU-only builds.
# Ubuntu 20.04 (focal). Run as a sudo-capable user. Safe to re-run.
# Not enabled: libwavpack (removed from FFmpeg 5+, native decoder used),
# chromaprint (focal's library links FFmpeg 4.2, loading two FFmpegs at once).
set -euo pipefail

FFMPEG_VERSION=${FFMPEG_VERSION:-7.1.3}
VULKAN_HEADERS_TAG=${VULKAN_HEADERS_TAG:-v1.4.347}
AOM_TAG=${AOM_TAG:-v3.15.2}                 # focal has 1.0, FFmpeg 7 needs 2.0+
DAV1D_TAG=${DAV1D_TAG:-1.4.3}               # focal has no libdav1d-dev
NV_CODEC_TAG=${NV_CODEC_TAG:-n12.1.14.0}   # NVIDIA driver 530+ for NVENC
PREFIX=${PREFIX:-/opt/ffmpeg-${FFMPEG_VERSION}}
VULKAN_PREFIX=${VULKAN_PREFIX:-/opt/vulkan-headers}
SRC=${SRC:-/opt/src}
JOBS=${JOBS:-$(nproc)}
SKIP_APT=${SKIP_APT:-0}

export DEBIAN_FRONTEND=noninteractive
if [ "$SKIP_APT" != 1 ]; then
sudo apt-get update
sudo apt-get install -y --no-install-recommends \
  build-essential pkg-config nasm yasm git curl xz-utils cmake \
  glslang-tools libvulkan-dev \
  libx264-dev libx265-dev libvpx-dev meson ninja-build \
  libmp3lame-dev libopus-dev libvorbis-dev libtheora-dev libspeex-dev \
  libtwolame-dev libshine-dev libwavpack-dev libgsm1-dev libcodec2-dev \
  libxvidcore-dev libwebp-dev libopenjp2-7-dev libsnappy-dev libsoxr-dev \
  libass-dev libfreetype6-dev libfontconfig1-dev libfribidi-dev libxml2-dev \
  libbluray-dev libbs2b-dev libcaca-dev libcdio-paranoia-dev flite1-dev \
  libgme-dev libmysofa-dev libopenmpt-dev librsvg2-dev librubberband-dev \
  libssh-dev libgnutls28-dev libvidstab-dev libzmq3-dev libzvbi-dev \
  lv2-dev liblilv-dev ladspa-sdk frei0r-plugins-dev \
  libpulse-dev libjack-jackd2-dev libopenal-dev libsdl2-dev \
  ocl-icd-opencl-dev opencl-headers libgl-dev \
  libdc1394-22-dev libiec61883-dev libavc1394-dev libraw1394-dev \
  libdrm-dev libva-dev libvdpau-dev
fi

sudo mkdir -p "$SRC" && sudo chown "$(id -u):$(id -g)" "$SRC"
cd "$SRC"

# Vulkan headers: focal has 1.2.131, FFmpeg 7 needs 1.3.277+. Header-only.
if [ ! -f "$VULKAN_PREFIX/include/vulkan/vulkan_core.h" ]; then
  rm -rf Vulkan-Headers
  git clone --depth 1 --branch "$VULKAN_HEADERS_TAG" https://github.com/KhronosGroup/Vulkan-Headers.git
  # Copy directly: its CMakeLists needs CMake 3.22, focal has 3.16
  sudo mkdir -p "$VULKAN_PREFIX"
  sudo cp -r Vulkan-Headers/include "$VULKAN_PREFIX/"
fi

# NVENC/CUDA headers (header-only, loaded at runtime from the driver)
if [ ! -f "$PREFIX/lib/pkgconfig/ffnvcodec.pc" ]; then
  rm -rf nv-codec-headers
  git clone --depth 1 --branch "$NV_CODEC_TAG" https://github.com/FFmpeg/nv-codec-headers.git
  sudo make -C nv-codec-headers install PREFIX="$PREFIX"
fi

# Intel oneVPL dispatcher (supports both oneVPL and legacy Media SDK runtimes).
# The dispatcher is bundled; GPU runtimes/drivers remain supplied by the host.
if [ ! -f "$PREFIX/lib/pkgconfig/vpl.pc" ]; then
  archive=libvpl-2.10.2.tar.gz
  if [ ! -f "$archive" ]; then
    curl -fsSL --retry 2 https://github.com/intel/libvpl/archive/refs/tags/v2.10.2.tar.gz -o "$archive"
  fi
  printf '%s  %s\n' ad956ea7ecf14614325f59dfb44cc5ba08e2fcac373342d61c7db152ac651253 "$archive" | sha256sum -c -
  mkdir -p libvpl-2.10.2
  tar xzf "$archive" --strip-components=1 -C libvpl-2.10.2
  cmake -S libvpl-2.10.2 -B libvpl-2.10.2/build -DCMAKE_BUILD_TYPE=Release \
    -DCMAKE_INSTALL_PREFIX="$PREFIX" -DCMAKE_INSTALL_LIBDIR=lib -DBUILD_SHARED_LIBS=ON \
    -DBUILD_TESTS=OFF -DBUILD_TOOLS=OFF -DBUILD_EXAMPLES=OFF -DINSTALL_EXAMPLE_CODE=OFF \
    -DBUILD_DISPATCHER_ONEVPL_EXPERIMENTAL=OFF
  cmake --build libvpl-2.10.2/build --parallel "$JOBS"
  sudo cmake --install libvpl-2.10.2/build
fi

# AMD AMF headers, pinned to the 1.4.35 release commit and verified individually.
# FFmpeg loads the AMF runtime from the installed AMD driver at export time.
amf="$PREFIX/include/AMF"
sudo mkdir -p "$amf"
while read -r digest header; do
  if ! printf '%s  %s\n' "$digest" "$amf/$header" | sha256sum -c - >/dev/null 2>&1; then
    mkdir -p "amf-headers/$(dirname "$header")"
    curl -fsSL --retry 2 "https://raw.githubusercontent.com/GPUOpen-LibrariesAndSDKs/AMF/ba07d1b3c7c7ee0d846125cffc1d6d78a020c45a/amf/public/include/$header" -o "amf-headers/$header"
    printf '%s  %s\n' "$digest" "amf-headers/$header" | sha256sum -c -
    sudo mkdir -p "$amf/$(dirname "$header")"
    sudo cp "amf-headers/$header" "$amf/$header"
  fi
done <<'AMF_HEADERS'
70afd5279be90e656c2ec270229c5a702a11435a670052370e96b270e6b7640f  components/AMFXInput.h
27770ae77d2f7a16eca9ab1f8b9ce87cf0c0350425b950bdd39189c2c91a519a  components/Ambisonic2SRenderer.h
05a9ebc32217e69b6c4659f4744dbda9e5a2792f3e56bc1bcf61c15dcfd46cab  components/AudioCapture.h
bbe91a1e744685a36269d3b6af9cd36d9716ee748c8d0ed95255a6a42b46892f  components/Capture.h
a3a89490d44adaffc2fa2cccc14313f8f65e5af7a6431d89d85bd80e36c628a1  components/ChromaKey.h
2f6887d14516ebdcf034d59e39235f5d9974533ddb0b04d685c0c3b6d397d3d6  components/ColorSpace.h
f3f18680d55a65fdf5e0351d3fde1dcd9bdd2d6caec268c8c915c4227ffa1edc  components/Component.h
2479eb2a6174a1b63552c509abac1b701eddcab5a5eac5a7f4d9f40274d588b4  components/ComponentCaps.h
7c22337ff313320bd7fe20ef0fb00756735012367a5daa7c621725030f578863  components/CursorCapture.h
693c8e491f90eaffd226c1f6b2da37d3a5ef1d3c610e554647f7d0b699832727  components/DisplayCapture.h
9dde7e850c8bdecda82b0cee1b331294f6cc91b32b1d1504344a023cd4dda8af  components/FFMPEGAudioConverter.h
92f50d789bc18d5ebd143b2925e124594a89110eef7efaa8189eb063bf2f3aa8  components/FFMPEGAudioDecoder.h
2493067de9b3f540ffaae8a6466e57ef75af4772f037a81785b11ee9511029f5  components/FFMPEGAudioEncoder.h
87a9ae9480c3af6210b1282d1768e36673fcc1ef4970afa6edaa74481cf0831b  components/FFMPEGComponents.h
5d64b7d2d50ca17ba601885dcdb51bb16ef4dccf88323437967e8524f26194bc  components/FFMPEGEncoderAV1.h
3dae38db05bc55af525000c28559ff76625e0cddc2ad2c4c2af52eab69e915bc  components/FFMPEGEncoderH264.h
3ad5cc5d3284b253a671499efa29c7a46b83a6e6373aad7142c7adcbfae31875  components/FFMPEGEncoderHEVC.h
be992e14086b04e2a7b5503471a6c262c58fda163c45ad6d00f8e425d9f4a820  components/FFMPEGFileDemuxer.h
3fefbe1aa7aaa4c000a97efb2e5ba26dc87ec15d11b07ceb0f43c768a1d589a2  components/FFMPEGFileMuxer.h
297b0ab2291baeb44f30f03ffbf791a5603289ab34f625d60a33150f2be1f7b6  components/FFMPEGVideoDecoder.h
ce5fb69c0546dd08dff0ee680972c5ec5a0b2d9b95e509338358eb2827f30221  components/FRC.h
b63e856b55db7a6853f444a6d4d32cfe006d9e0fb1d9be1b3079557534738591  components/HQScaler.h
c4e168d29e3f7d9318435958c3c784ad4c497159a617bd7a5598a2951b1b15f6  components/MediaSource.h
dae3564f20ef5db959d31dfc74c226453b039f4919ddb65027a183cdae95c171  components/PreAnalysis.h
5a01d71e128cf8bcaa4945fe76d9aad0156c409df3b25616a75645ef15972dc3  components/PreProcessing.h
550900e56c8254bf9c42d1b1d6538a390f7f7d3bf4826ee385b019585cb6cd22  components/SupportedCodecs.h
146e7cd35acc846f6b75b696bbe13a5d790f885bc23d9913e7de66f515da789b  components/VQEnhancer.h
08f0162a631833f847d1b4b751e41e28b147a79ed19545b43631222d22e74507  components/VideoCapture.h
b0e173fbfe920a4923f461c1bffc73d1cbc06d54b676647dcd6ec300a0beee65  components/VideoConverter.h
588622f7d945c85c1ed334b497174fd46031e01664338bcf59c4b242920df886  components/VideoDecoderUVD.h
0b43ba9a0bb0d701c117e038d07270907d840ea2d2b13655fec71eda5e96d562  components/VideoEncoderAV1.h
b7827d2c3380a32cef350cf0f180802694f9f93db295f1c8e08238160d17c3d8  components/VideoEncoderHEVC.h
d106ce15028a9ea42e7299cff4b1a8697614a746b96740c9251f9b943bac2edc  components/VideoEncoderVCE.h
9bc2d8477d67693b0cd4d433fbc6a5812277190f67be5f188f0206b463f256ec  components/VideoStitch.h
3c6e6515a8fec111eb1847e349124a8c66c1e78be2d148bc0b8f85b9d9994e2d  components/ZCamLiveStream.h
c9382c911fd3b5e12105c1d2a67eb11e62e9475068825222a285bb1314ac0ddd  core/AudioBuffer.h
1ad18d3c813c3f81a836d393bbe86dfc0b7a1d7f04a77a272eb1461ce85ae3a4  core/Buffer.h
ef5721466fe48e1c038151018699519761e6e351e66e76a5de65386924653e20  core/Compute.h
50acf69fb670bc6cf093384d1b698bf9e44cda8dd49e02ef24eacfb86ad8ccae  core/ComputeFactory.h
f9b4acb784038aad234ddec490e5f6ad6dacc4b14794eb84f3523d56dc3d6dd2  core/Context.h
76d16ba57d48ae9e457f3d904698a1e7c625007ac8887db9bcecdb90fd36d835  core/CurrentTime.h
f05771305598b534e1efa0dd28825267ea87418c477b198e373f6764d060c666  core/D3D12AMF.h
eaac4d80dd3215638c70f8a4ca01791d26d9df77586f665a772bb51907a8035b  core/Data.h
56666589bf6ad20c6286a048da751c2d7722f70c73d962ee3c205a6d709b6aff  core/Debug.h
a5e0fa9290cae767a4d135962ee5e30302e65e8d0ca5e016c620e72b759c86c6  core/Dump.h
9ba2ebe93af628bce621f29de493cddab87bf8a030eed504b25fcdc822c61b44  core/Factory.h
304f6d6077e9b0733859d44a5e43a343ae374c8fe72c7d79741c0aca89c21162  core/Interface.h
5ad5165e9de2afcfc2da828e8704c838b1f82e8ead7422fbcfc32c758672e1f3  core/Plane.h
59a4d11ecd150af25ee0b8d1d15f2efdfe5cf1060b8e8ab539f6a46d4bff8bc8  core/Platform.h
0059b787e78cd7c4e57212ccaab56ad7e17e9474a31337da0eac5b2d056a5dd8  core/PropertyStorage.h
871136e46586baa1fc77121935b503aaa16523d006c3e435ce50698605675fad  core/PropertyStorageEx.h
a469ff33875484b5762199da757819056ff4a7535bfdcec1f4cf2ffc403b98f2  core/Result.h
7f0c99bbfe156169440031f75b0e37c28c435dd140ecb4e5089d0b1f01912c47  core/Surface.h
ae8c5fee671f02b90d3ccab312cfa6f74424351f070a7dd4ed7aba437fd55c30  core/Trace.h
72841e751595a2a8f62c8ac4a3e630b181411af8e0741d716bbe3379c9bf9a99  core/Variant.h
e26e25e658513f7120bb16d64d753d99205b456d83a1120a0a0d2ffdbf19f8c5  core/Version.h
385b8ad96b59e2d67a6f7a78c6aa660a2f2cdad1bf8c7cadcd83387a3ad4996c  core/VulkanAMF.h
AMF_HEADERS

# dav1d (fast AV1 decoder), built into the FFmpeg prefix
if [ ! -f "$PREFIX/lib/pkgconfig/dav1d.pc" ]; then
  rm -rf dav1d
  git clone --depth 1 --branch "$DAV1D_TAG" https://code.videolan.org/videolan/dav1d.git
  meson setup dav1d/build dav1d --prefix="$PREFIX" --libdir=lib --buildtype=release \
    -Denable_tools=false -Denable_tests=false
  ninja -C dav1d/build
  sudo ninja -C dav1d/build install
fi

# libaom (AV1 encoding), built into the FFmpeg prefix
if [ ! -f "$PREFIX/lib/pkgconfig/aom.pc" ]; then
  rm -rf aom
  git clone --depth 1 --branch "$AOM_TAG" https://aomedia.googlesource.com/aom
  cmake -S aom -B aom/build-out -DCMAKE_BUILD_TYPE=Release -DCMAKE_INSTALL_PREFIX="$PREFIX" \
    -DCMAKE_INSTALL_LIBDIR=lib -DBUILD_SHARED_LIBS=1 \
    -DENABLE_TESTS=0 -DENABLE_EXAMPLES=0 -DENABLE_DOCS=0 -DENABLE_TOOLS=0
  cmake --build aom/build-out --parallel "$JOBS"
  sudo cmake --install aom/build-out
fi

if [ ! -d "ffmpeg-$FFMPEG_VERSION" ]; then
  # GitHub mirror: ffmpeg.org is not always reachable from the runners
  archive="FFmpeg-n$FFMPEG_VERSION.tar.gz"
  curl -fsSL --retry 2 "https://github.com/FFmpeg/FFmpeg/archive/refs/tags/n$FFMPEG_VERSION.tar.gz" -o "$archive"
  printf '%s  %s\n' "${FFMPEG_SHA256:-e0b04c4b43d7e6d67cb6710334fb513adf13ac860532f30e1d0ac4c231f232fb}" "$archive" | sha256sum -c -
  tar xzf "$archive"
  mv "FFmpeg-n$FFMPEG_VERSION" "ffmpeg-$FFMPEG_VERSION"
fi
cd "ffmpeg-$FFMPEG_VERSION"
export PKG_CONFIG_PATH="$PREFIX/lib/pkgconfig"
./configure --prefix="$PREFIX" --enable-gpl --enable-version3 \
  --enable-shared --disable-static --disable-doc \
  --extra-cflags="-I$VULKAN_PREFIX/include -I$PREFIX/include" \
  --extra-ldflags="-L$PREFIX/lib" \
  --enable-vulkan --enable-ffnvcodec --enable-nvenc --enable-nvdec --enable-cuvid \
  --enable-libvpl --enable-amf \
  --enable-vaapi --enable-vdpau --enable-libdrm --enable-opencl --enable-opengl \
  --enable-libx264 --enable-libx265 --enable-libvpx --enable-libaom --enable-libdav1d \
  --enable-libmp3lame --enable-libopus --enable-libvorbis --enable-libtheora \
  --enable-libspeex --enable-libtwolame --enable-libshine \
  --enable-libgsm --enable-libcodec2 --enable-libxvid --enable-libwebp \
  --enable-libopenjpeg --enable-libsnappy --enable-libsoxr \
  --enable-libass --enable-libfreetype --enable-libfontconfig --enable-libfribidi \
  --enable-libxml2 --enable-libbluray --enable-libbs2b --enable-libcaca \
  --enable-libcdio --enable-libflite --enable-libgme --enable-libmysofa \
  --enable-libopenmpt --enable-librsvg --enable-librubberband --enable-libssh \
  --enable-gnutls --enable-libvidstab --enable-libzmq --enable-libzvbi \
  --enable-lv2 --enable-ladspa --enable-frei0r \
  --enable-libpulse --enable-libjack --enable-openal --enable-sdl2 \
  --enable-libdc1394 --enable-libiec61883
make -j"$JOBS"
sudo make install

# Build env for GPU jobs: source this before cmake.
sudo tee "$PREFIX/env.sh" >/dev/null <<ENV
export FFMPEG_ROOT=$PREFIX
export PKG_CONFIG_PATH=$PREFIX/lib/pkgconfig:\${PKG_CONFIG_PATH:-}
export LD_LIBRARY_PATH=$PREFIX/lib:\${LD_LIBRARY_PATH:-}
export CMAKE_PREFIX_PATH=$PREFIX:\${CMAKE_PREFIX_PATH:-}
export VULKAN_HEADERS_INCLUDE=$VULKAN_PREFIX/include
ENV
LD_LIBRARY_PATH="$PREFIX/lib" "$PREFIX/bin/ffmpeg" -hide_banner -version | head -1
LD_LIBRARY_PATH="$PREFIX/lib" "$PREFIX/bin/ffmpeg" -hide_banner -hwaccels
LD_LIBRARY_PATH="$PREFIX/lib" "$PREFIX/bin/ffmpeg" -hide_banner -encoders > "$PREFIX/compiled-encoders.txt" 2>&1
for codec in h264_nvenc hevc_nvenc h264_qsv hevc_qsv h264_amf hevc_amf h264_vaapi hevc_vaapi h264_vulkan hevc_vulkan; do
  grep -Eq "[[:space:]]$codec[[:space:]]" "$PREFIX/compiled-encoders.txt" || { echo "Required encoder missing: $codec"; exit 1; }
done
echo "Done: source $PREFIX/env.sh"
