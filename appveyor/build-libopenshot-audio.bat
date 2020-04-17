@echo on
git clone https://github.com/OpenShot/libopenshot-audio
mkdir libopenshot-audio-build-%PLAT_ARCH%
cd libopenshot-audio-build-%PLAT_ARCH%
cmake -DCMAKE_INSTALL_PREFIX=%APPVEYOR_BUILD_FOLDER%\install-%PLAT_ARCH% -DCMAKE_BUILD_TYPE=Debug -G "MinGW Makefiles" ..\libopenshot-audio
cmake --build . -- VERBOSE=1
cmake --build . --target install
cd ..
