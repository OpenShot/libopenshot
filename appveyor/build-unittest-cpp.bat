@echo on
git clone https://github.com/unittest-cpp/unittest-cpp
mkdir unittest-cpp-build
cd unittest-cpp-build
make -DCMAKE_INSTALL_PREFIX=%APPVEYOR_BUILD_FOLDER%\install-%PLAT_ARCH% -G "MinGW Makefiles" ..\unittest-cpp
cmake --build . -- VERBOSE=1
cmake --build . --target install
cd ..
