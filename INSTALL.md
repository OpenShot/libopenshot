## Detailed Install Instructions

Operating system specific install instructions are located in:

*   [doc/INSTALL-LINUX.md][INSTALL-LINUX]
*   [doc/INSTALL-MAC.md][INSTALL-MAC]
*   [doc/INSTALL-WINDOWS.md][INSTALL-WINDOWS]

## Getting Started

The best way to get started with libopenshot is to learn about our build system,
obtain all the source code, install a development IDE and tools,
and better understand our dependencies.
So, please read through the following sections, and follow the instructions.
And keep in mind, your computer is likely different than the one used when writing these instructions.
Your file paths and versions of applications will be different,
so you should treat the specifics used here as examples and adjust as necessary.
Keep an eye out for subtle file path differences in the commands you type.

## Dependencies

The following libraries are required to build libopenshot.
Instructions on how to install these dependencies vary for each operating system.
Libraries and executables have been labeled in the list below to help distinguish between them.

#### FFmpeg (libavformat, libavcodec, libavutil, libavdevice, libavresample, libswscale)
*   <http://www.ffmpeg.org/> **(Library)**

*   This library is used to decode and encode video, audio, and image files.
    It is also used to obtain information about media files,
    such as frame rate, sample rate, aspect ratio, and other common attributes.

#### ImageMagick++ (libMagick++, libMagickWand, libMagickCore)
*   <http://www.imagemagick.org/script/magick++.php> **(Library)**
*   This library is **optional**, and used to decode and encode images.

#### OpenShot Audio Library (libopenshot-audio)
*   <https://github.com/OpenShot/libopenshot-audio/> **(Library)**

*   This library is used to mix, resample, host plug-ins, and play audio.
    It is based on the JUCE project,
    an outstanding audio library used by many different applications.

#### Qt 5 (libqt5)
*   <http://www.qt.io/qt5/> **(Library)**

*   Qt5 is used to display video, store image data, composite images,
    apply image effects, and many other utility functions,
    such as file system manipulation, high resolution timers, etc.

#### ZeroMQ (libzmq)
*   <http://zeromq.org/> **(Library)**

*   This library is used to communicate between libopenshot and other applications (publisher / subscriber).
    Primarily used to send debug data from libopenshot.

#### OpenMP (`-fopenmp`)
*   <http://openmp.org/wp/> **(Compiler Flag)**

*   If your compiler supports this flag (GCC, Clang, and most other compilers),
    it provides libopenshot with implementations of common parallel programming techniques
    used to improve performance and take advantage of multi-core processors.

#### CMake (`cmake`)
*   <http://www.cmake.org/> **(Executable)**

*   This executable is used to automate the generation of Makefiles,
    check for dependencies, and is the backbone of libopenshot’s cross-platform build process.

#### SWIG (`swig`)
*   <http://www.swig.org/> **(Executable)**

*   This executable is used to generate the Python and Ruby bindings for libopenshot.
    It is a powerful wrapper for C++ libraries, and supports many languages.

#### Python 3 (libpython)
*   <http://www.python.org/> **(Executable and Library)**

*   This library is used by swig to create the Python (version 3+) bindings for libopenshot.
    This is also the official language used by OpenShot Video Editor (a graphical interface to libopenshot).

#### Doxygen (doxygen)
*   <http://www.stack.nl/~dimitri/doxygen/> **(Executable)**
*   This executable is used to auto-generate the documentation used by libopenshot.

#### UnitTest++ (libunittest++)
*   <https://github.com/unittest-cpp/> **(Library)**

*   This library is used to execute unit tests for libopenshot.
    It contains many macros used to keep our unit testing code clean and easy-to-follow.

## Obtaining Source Code

The first step in installing libopenshot is to obtain the most recent source code.
The source code is available on [GitHub](https://github.com/OpenShot/libopenshot).
Use the following command to obtain the latest libopenshot source code.

```sh
git clone https://github.com/OpenShot/libopenshot.git
git clone https://github.com/OpenShot/libopenshot-audio.git
```

### Folder Structure (libopenshot)

The source code is divided up into the following folders.

#### `build/`
This folder needs to be manually created,
and is used by cmake to store the build system control files and generated output
(such as compiled object files and the result of template-file processing)
as well as the final results of the build (library, tool, and test program binaries).

#### `cmake/`
This folder contains custom modules not included by default in cmake.
CMake find modules are used to discover dependency libraries on the system,
and to incorporate their headers and object files.
CMake code modules are used to implement build features such as test coverage scanning.

#### `doc/`
This folder contains documentation and related files.
This includes logos and images required by the doxygen-generated API documentation.

#### `src/`
This folder contains all source code (`*.cpp`) and headers (`*.h`) for libopenshot.

#### `bindings/`
This folder contains language bindings for the libopenshot API.
Current supported languages are Python and Ruby.

#### `examples/`
This folder contains various pieces of example code written in C++, Ruby, or Python.
It also holds the media files (data files) used in examples and unit tests.

#### `tests/`
This folder contains all unit test code.
Each test file (`<class>_Tests.cpp`) contains the tests for the named class.
We use UnitTest++ macros to keep the test code uncomplicated and manageable.

#### `thirdparty/`
This folder contains code not written by the OpenShot team.
For example, `jsoncpp`, an open-source JSON parser.

## Build Tools

CMake is the backbone of our build system.
It is a cross-platform build system, which checks for dependencies,
locates header files and libraries, and generates a build system in various formats.
We use CMake's Makefile generators to compile libopenshot and libopenshot-audio.

CMake uses an out-of-source build concept.
This means that the build system,
along with all temporary/intermediate files and generated products output during the build,
is kept strictly separate from the source code.
This includes Makefiles, object files, and even the final binaries.
The source code files and directories are never modified by the build system,
which makes it convenient and safe to re-run, reconfigure, or discard builds as needed.
While it is possible to build in-tree,
we highly recommend you use a `/build/` sub-folder to compile each library.

These instructions have only been tested with the following compiler stacks:
*   The GNU compiler suite (including MSYS2/MinGW for Windows)
*   The Clang compiler (including AppleClang on MacOS)

Other compilers, including MSVC, are entirely unsupported.
It may be possible to build libopenshot using other compiler stacks,
but most likely not without modifications to the build system which you would have to make yourself.

### CMake Flags (Optional)
There are many different build flags that can be passed to cmake to adjust how libopenshot is compiled.
Some of these flags might be required when compiling on certain OSes,
depending on how your build environment is setup.

To add a build flag, follow this general syntax:

```sh
cmake -B build -S . -DMAGICKCORE_HDRI_ENABLE=1 -DENABLE_TESTS=1 ...
```

Following are some of the flags you might need to set when generating your build system.

#### Optional behaviors of the build system
*   `-DENABLE_TESTS=0` (default: `ON`)
*   `-DENABLE_COVERAGE=1` (default: `OFF`)
*   `-DENABLE_DOCS=0` (default: `ON` if doxygen found)
*   `-DENABLE_RUBY=0` (default: `ON` if SWIG and Ruby detected)
*   `-DENABLE_PYTHON=0` (default: `ON` if SWIG and Python detected)

#### Options to configure the compiler
*   `-DCMAKE_BUILD_TYPE=Release`, `-DCMAKE_BUILD_TYPE=Debug` (default: `Release` if unset)
*   `-DCMAKE_CXX_FLAGS="-Wall -Wextra"` (default: CMake builtin defaults for build type)
*   `-DCMAKE_CXX_COMPILER=/path/to/g++`, `-DCMAKE_CXX_COMPILER=/path/to/clang++`
*   `-DCMAKE_C_COMPILER=/path/to/gcc`, `-DCMAKE_CXX_COMPILER=/path/to/clang` (used by CMake for OS probes)

#### Options to configure dependencies
*   `-DCMAKE_PREFIX_PATH=/extra/path/to/search/for/libraries/`
*   `-DUSE_SYSTEM_JSONCPP=0` (default: auto if discovered)
*   `-DENABLE_MAGICK=0` (default: auto if discovered)

#### Options to compile bindings for a specific Python installation
*   `-DPYTHON_INCLUDE_DIR=/location/of/python/includes/`
*   `-DPYTHON_LIBRARY=/location/of/libpython*.so`
*   `-DPYTHON_FRAMEWORKS=/usr/local/Cellar/python3/3.3.2/Frameworks/Python.framework/` (MacOS only)

#### Options only relevant when building with ImageMagick
*   `-DMAGICKCORE_HDRI_ENABLE=1` (default `0`)
*   `-DMAGICKCORE_QUANTUM_DEPTH=8` (default `16`)

## Linux Build Instructions (libopenshot-audio)
To compile libopenshot-audio, we need to build it from source code and install the results.
Launch a terminal and enter:

```sh
cd [libopenshot-audio repo folder]
cmake -B build -S .
cmake --build build
./build/src/openshot-audio-test-sound  (This should play a test sound)
cmake --install build
```

## Linux Build Instructions (libopenshot)
Run the following commands to compile libopenshot:

```sh
cd [libopenshot repo directory]
cmake -B build -S .
cmake --build build
cmake --build build --target test
cmake --install build
```

For more detailed instructions, please see:

*   [doc/INSTALL-LINUX.md][INSTALL-LINUX]
*   [doc/INSTALL-MAC.md][INSTALL-MAC]
*   [doc/INSTALL-WINDOWS.md][INSTALL-WINDOWS]

[INSTALL-LINUX]: https://github.com/OpenShot/libopenshot/blob/develop/doc/INSTALL-LINUX.md
[INSTALL-MAC]: https://github.com/OpenShot/libopenshot/blob/develop/doc/INSTALL-MAC.md
[INSTALL-WINDOWS]: https://github.com/OpenShot/libopenshot/blob/develop/doc/INSTALL-WINDOWS.md
