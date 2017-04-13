#!/usr/bin/env bash
set -e

export BASE_DIR="$(dirname $(pwd))"
export INSTALL_PATH="$BASE_DIR/local"

# Install dependencies

sudo apt update
sudo apt install --yes cmake libx11-dev build-essential cmake libavformat-dev libavdevice-dev libswscale-dev libavresample-dev libavutil-dev libmagick++-dev libunittest++-dev swig doxygen libxinerama-dev libxcursor-dev libasound2-dev libxrandr-dev libzmq3-dev git wget mesa-common-dev ruby-dev python3-dev

# Install Qt
sudo apt install --yes software-properties-common
sudo add-apt-repository --yes ppa:beineri/opt-qt562-trusty
sudo apt update
sudo apt install --yes qt563d qt56base qt56canvas3d qt56connectivity qt56declarative qt56graphicaleffects qt56imageformats qt56location qt56multimedia qt56qbs qt56quickcontrols qt56quickcontrols2 qt56script qt56sensors qt56serialbus qt56serialport qt56svg qt56tools qt56translations qt56wayland qt56webchannel qt56webengine qt56websockets qt56x11extras qt56xmlpatterns
source "/opt/qt56/bin/qt56-env.sh"

# Install libopenshot-audio

export PROJECT_NAME="libopenshot-audio"
cd "$BASE_DIR"
wget "https://github.com/OpenShot/libopenshot-audio/archive/v0.1.2.tar.gz" -O "$PROJECT_NAME.tar.gz"
tar -xvzf "$PROJECT_NAME.tar.gz"
ln -s $PROJECT_NAME-* $PROJECT_NAME

cd "$BASE_DIR/$PROJECT_NAME"
mkdir -p build/ && cd build/
cmake -DCMAKE_INSTALL_PREFIX=$INSTALL_PATH ..
make && make install

# Install libopenshot

export PROJECT_NAME="libopenshot"
export NAME_BIN_FOLDER="$PROJECT_NAME-bin"
export GIT_HASH=$(git rev-parse --short HEAD)

mkdir -p $INSTALL_PATH
cd "$BASE_DIR/$PROJECT_NAME"
mkdir -p build/ && cd build/
cmake -DCMAKE_INSTALL_PREFIX=$INSTALL_PATH ..
make && make install

cd $INSTALL_PATH
tar -cv . | xz -9 -c - > "$(dirname $(pwd))/$PROJECT_NAME-$GIT_HASH.tar.xz"

cd "$BASE_DIR"
ls
