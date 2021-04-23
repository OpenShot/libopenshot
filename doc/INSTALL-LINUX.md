# Building libopenshot for Linux

## Getting Started

The best way to get started with libopenshot is by trying it yourself.
Obtain the source code and learn about our build system,
to better understand our dependencies.
Then, install a development IDE and tools to build and work with the code.
Thhe instructions in this document will guide you through that process.
But keep in mind,
your computer will be different from the one used to write these instructions.
File paths, software versions, and other details will differ slightly.
You should keep an eye out for any changes you need to make,
before copy-pasting the commands listed here.

## Build Tools

CMake is the backbone of our build system.
It is a cross-platform build system, used on all supported operating systems.
CMake checks for dependencies,
locates header files and libraries,
generates build scripting and configuration,
and drives cross-platform compiling of libopenshot and libopenshot-audio.

CMake uses an out-of-source build concept.
All generated files, whether build tooling or compiled binaries,
are placed in a location separate from the source code they were built from.
Typically, we use a sub-folder named `build/` at the root of the tree.
This separation prevents the build process from cluttering up the source code.

These instructions have only been tested with the GNU C++ compiler `g++`
(including under MSYS2/MinGW for Windows),
and the Clang C++ compiler `clang++`
(including AppleClang on macOS systems).

## Dependencies

The following libraries are required to build libopenshot.
Installation instructions vary for each operating system.
Each item in the list has been labeled as a Library or Executable,
to help distinguish between the two major types of dependency.

### FFmpeg (libavformat, libavcodec, libavutil, libavdevice, libavresample, libswscale)

*   http://www.ffmpeg.org/ **(Library)**
*   Used to decode and encode video, audio, and image files.
    It is also used to obtain information about media files,
    such as frame rate, sample rate, aspect ratio, and other common attributes.

### ImageMagick++ (libMagick++, libMagickWand, libMagickCore)

*   http://www.imagemagick.org/script/magick++.php **(*Optional* Library)**
*   Can be used to decode and encode images. (Largely replaced by Qt5's QImage.)

### OpenShot Audio Library (libopenshot-audio)

*   https://github.com/OpenShot/libopenshot-audio/ **(Library)**
*   The companion library to libopenshot,
    it's used to mix, resample, and play audio.
    libopenshot-audio is based on the JUCE project,
    an outstanding audio library used by many different applications.

### Qt 5 (libQt5Core, libQt5Gui, libQt5Widgets)

*   http://www.qt.io/qt5/ **(Library)**
*   Used for a wealth of internal data processing and utility functions.
    These include: displaying video; storing and processing image data;
    applying visual effects; and data, string, and file manipulation.

### CMake (cmake)

*   http://www.cmake.org/ **(Executable)**
*   Used to generate the build system that compiles libopenshot.
    CMake detects and configures all of the other build dependencies,
    scripts the compiler and linker commands to produce executable code,
    and generates libopenshot's final configuration and install tree.

### SWIG (swig)

*   http://www.swig.org/ **(Executable)**
*   Used during the build process to generate language binding wrappers,
    allowing us to produce Python and Ruby interfaces to libopenshot.
    SWIG supports many more languages,
    and libopenshot could be extended to any of them if necessary.

### Python 3 (libpython)

*   http://www.python.org/ **(Executable and Library)**
*   SWIG compiles libopenshot's Python bindings as extensions to libpython,
    making them accessible from any Python code running in the interpreter.
    Python is also the language used by the primary consumer of our APIs,
    OpenShot Video Editor (a PyQt5 graphical interface to libopenshot).

### Doxygen (doxygen)

*   http://www.stack.nl/~dimitri/doxygen/ **(Executable)**
*   Used to auto-generate the API documentation for libopenshot.

### Catch2 (catch2.hpp) & CTest (ctest)

*   Catch2: https://github.com/catchorg/catch2/ **(Header-only Library)**
*   CTest: Part of the CMake build system
*   Used together to ensure correctness of our code through unit testing.
    Catch2 provides macros used to write clean and simple tests.
    CTest unit-tests each code submission and reports on the results,
    which help us to catch many bugs before they make it into the library.

### ZeroMQ (libzmq)

*   http://zeromq.org/ **(Library)**
*   Used to communicate between libopenshot and other applications,
    using a publisher / subscriber local-networking model.
    Primarily used to stream libopenshot's debug logs to library users.

### OpenMP (-fopenmp)

*   http://openmp.org/wp/ **(Compiler Flag)**
*   Compilers that support this flag (including GCC, Clang, and most others)
    allow libopenshot to easily activate parallel coding techniques,
    permitting our code to take best advantage of multi-core processors.
    These algorithms are primarily used to accelerate effects processing.

## CMake Flags (Optional)

The libopenshot build system offers many customization options,
which can be used to adjust for specific scenarios or environments.
Some may be required when compiling on certain OSes,
or depending how your build environment is setup.
What follows is an incomplete list,
many additional options are currently undocumented.
For full details read the `CMakeLists.txt` files in the source tree.

CMake options are set using either the `ccmake` graphical tool
(which is not covered here),
or on the `cmake` command line using this general syntax:

```sh
cmake -DENABLE_SOME_OPTION=1 -DSOME_DIRECTORY=/path/to/files
```

### Build configuration

*   `BUILD_TESTING` (default: `1`/`TRUE`)
*   `CMAKE_INSTALL_PREFIX` (default: `/usr/local`)
*   `CMAKE_PREFIX_PATH` (e.g. `/location/to/missing/library/`)
*   `CMAKE_CXX_COMPILER` (e.g. `/location/of/g++`, or `g++` or `clang++`)
*   `CMAKE_C_COMPILER` (e.g. `/location/of/gcc` or `gcc` or `clang`)

### Dependencies

*   `OpenShotAudio_ROOT` (e.g. `/install/prefix/of/libopenshot-audio`)
*   `QT_DIR` (e.g. `/location/of/Qt/5.15.0/gcc_x86_64/bin`, path to `qmake`)
*   `PYTHON_INCLUDE_DIR` (e.g. `/location/to/python/include/`)
*   `PYTHON_LIBRARY` (e.g. `/location/of/libpython.so`, or `.dll` or `.dylib`)
*   `PYTHON_FRAMEWORKS` (e.g. `/usr/local/Cellar/python3/3.8.9/Frameworks/Python.framework`)

#### Only applicable if ImageMagick is used

*   `MAGICKCORE_HDRI_ENABLE` (default: `0`)
*   `MAGICKCORE_QUANTUM_DEPTH` (default: `16`)

## Obtaining Source Code

The first step in installing libopenshot is to obtain the most recent code.
The source code is maintained in the official GitHub project
[OpenShot/libopenshot](https://github.com/OpenShot/libopenshot).
Use the following commands to obtain a local copy of the current code.

```sh
git clone https://github.com/OpenShot/libopenshot.git
git clone https://github.com/OpenShot/libopenshot-audio.git
```

## Folder Structure (libopenshot)

The source code is divided up into the following folders.

### `build/`

*   This folder needs to be manually created,
    and is used by cmake to store the temporary build files,
    as well as the final binaries (library and test executables).

### `cmake/`

*   This folder contains custom modules not included by default in cmake.
    These are used to find dependency libraries and headers,
    and to configure the build based on how they're installed.

### `doc/`

*   This folder contains documentation and related files,
    such as logos and images used in the auto-generated documentation.

### `src/`

*   This folder contains the C++ source (`*.cpp`) and headers (`*.h`)
    that make up the libopenshot code.

### `tests/`

*   This folder contains the unit test code.
    Each file (`ClassName.cpp`) contains the tests for that class.
    This helps keep the test code simple and manageable.

### `thirdparty/`

*   This folder contains code not written by the OpenShot team.
    Currently only a vendored copy of jsoncpp, an open-source JSON parser.
    CMake will automatically use that copy, by default,
    when jsoncpp is not installed on the operating system.

## Install Dependencies

In order to actually compile libopenshot,
we need to install some dependencies on your system.
The easiest way to accomplish this is with our Daily PPA.
A PPA is an unofficial Ubuntu repository,
which has our software packages available to download and install.

```sh
sudo add-apt-repository ppa:openshot.developers/libopenshot-daily
sudo apt update
sudo apt install    cmake \
                    libasound2-dev \
                    libopenshot-audio-dev \
                    libavcodec-dev \
                    libavformat-dev \
                    libavutil-dev \
                    libswresample-dev \
                    libswscale-dev \
                    libpostproc-dev \
                    qtbase5-dev \
                    qtbase5-dev-tools \
                    libjsoncpp-dev \
                    libzmq3-dev \
                    libopencv-dev \
                    libprotobuf-dev \
                    protobuf-compiler \
                    python3-dev \
                    swig
```

On Ubuntu 20.10+, Catch2 can be installed with `sudo apt install catch`.
Earlier systems only have Catch 1.x available,
so install Catch2 using these commands:

```sh
wget https://launchpad.net/ubuntu/+archive/primary/+files/catch2_2.13.0-1_all.deb
sudo dpkg -i catch2_2.13.0-1_all.deb
```

## Linux Build Instructions (libopenshot-audio)

To compile libopenshot-audio, in a terminal window type these commands:

```sh
cd /location/of/libopenshot-audio
cmake -B build -S .
cmake --build build
./build/src/openshot-audio-demo  # (This should play 5 test tones)
cmake --install build
```

## Linux Build Instructions (libopenshot)

Run the following commands to compile libopenshot:

```sh
cd /location/of/libopenshot
cmake -B build -S .
cmake --build build
cmake --build build --target test  # To run the unit tests
```

If you are missing any dependencies for libopenshot,
you might receive error messages at one of these points.
Just `apt install` the missing packages (usually with a -dev suffix),
and run the command again.
Repeat until no error messages are displayed and the build process completes.
Also, if you manually install Qt 5,
you might need to specify the location for cmake:

```sh
cmake -B build -S . -DCMAKE_PREFIX_PATH=/qt5_path/qt5/5.2.0/
```

To auto-generate documentation for libopenshot, after building the library run:

```sh
cmake --build build --target doc
```

This will use doxygen to generate a folder of HTML files,
with all classes and methods documented.
The folder is located at **`build/doc/html/`**.

Once libopenshot has been successfully built, we need to install it.

```sh
cmake --install build
```

This will copy the binary files to `/usr/local/lib/`,
and the header files to `/usr/local/include/libopenshot/`.
(Unless you customized the paths with `-DCMAKE_INSTALL_PREFIX=`.)

Python 3 bindings are also installed at this point.
Let's verify the python bindings work:

```python
python3
>>> import openshot
>>> openshot.Version
OpenShotVersion('0.2.5')
>>>
```

If the library's current version number is displayed,
you have successfully compiled and installed libopenshot on your system.
Congratulations, and welcome to the OpenShot developer community!

Be sure to read our wiki on [Becoming an OpenShot Developer](https://github.com/OpenShot/openshot-qt/wiki/Become-a-Developer).
We look forward to meeting you!
