FROM buildpack-deps:bookworm AS build_stage

WORKDIR /build

RUN git clone --depth=1 https://github.com/OpenShot/libopenshot.git && \
    git clone --depth=1 https://github.com/OpenShot/libopenshot-audio.git

RUN sed -i -e's/ main/ main contrib non-free non-free-firmware/g' /etc/apt/sources.list.d/debian.sources && \
    apt-get update && \
    apt-get install -y --no-install-recommends \
        cmake \
        pkg-config dpkg \
        libfreetype-dev \
        libasound2-dev \
        libavcodec-dev \
        libavformat-dev \
        libavutil-dev \
        libswresample-dev \
        libswscale-dev \
        libpostproc-dev \
        libfdk-aac-dev \
        libjsoncpp-dev \
        cppzmq-dev libzmq3-dev \
        qtbase5-dev \
        libqt5svg5-dev \
        libbabl-dev \
        libopencv-dev \
        libprotobuf-dev \
        protobuf-compiler \
        libpython3.11-dev \
        swig zlib1g \
        libgcc-s1 libstdc++6 \
        libomp-dev \
        libmagick++-6.q16-dev libmagick++-dev \
        && \
    apt-get clean && rm -rf /var/lib/apt/lists/*

ARG CMAKE_CXX_FLAGS="-O3 -flto=auto -ffast-math -fprofile-generate -fprofile-use -DNDEBUG -march=native -mtune=native -Wall -Wextra"

RUN cd libopenshot-audio && \
    cmake -DCMAKE_CXX_FLAGS="${CMAKE_CXX_FLAGS}" -B build -S . && cmake --build build -j$(nproc) && cmake --install build && \
    cd ../libopenshot && \
    cmake \
        -DCMAKE_CXX_FLAGS="${CMAKE_CXX_FLAGS}" \
        -DDISABLE_BUNDLED_JSONCPP="1" \
        -DENABLE_TESTS="0" \
        -DENABLE_COVERAGE="0" \
        -DCMAKE_BUILD_TYPE="Release" \
        -DENABLE_LIB_DOCS="0" \
        -B build -S . && \
    cmake --build build -j$(nproc)

FROM debian:bookworm-slim AS run_stage

ENV PYTHONDONTWRITEBYTECODE=1 \
    PYTHONUNBUFFERED=1 \
    DEBIAN_FRONTEND=noninteractive
    
WORKDIR /app

RUN --mount=from=build_stage,source=/build,target=/build,rw \
    sed -i -e's/ main/ main contrib non-free non-free-firmware/g' /etc/apt/sources.list.d/debian.sources && \
    apt-get update && \
    apt-get install -y --no-install-recommends \
        cmake \
        libfreetype6 \
        libasound2 \
        libavcodec59 \
        libavformat59 \
        libavutil57 \
        libswresample4 \
        libswscale6 \
        libpostproc56 \
        libfdk-aac2 \
        libjsoncpp25 \
        libzmq5 \
        libqt5core5a libqt5svg5 \
        gir1.2-babl-0.1 libbabl-0.1-0 \
        libopencv-core406 libopencv-contrib406 \
        libopencv-imgproc406 \
        libprotobuf32 \
        python3 libpython3.11 python3-pyqt5 \
        swig zlib1g \
        libc6 libstdc++6 \
        imagemagick-6-common libmagick++-6.q16-8 \
        libomp5-14 \
        ffmpeg \
        && \
    cd /build/libopenshot-audio && cmake --install build && \
    cd /build/libopenshot && cmake --install build && \
    ldconfig && \
    apt-get autoremove -y \
        cmake \
    && \
    apt-get clean && rm -rf /var/lib/apt/lists/*