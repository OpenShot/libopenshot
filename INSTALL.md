## Detailed Install Instructions

Operating system specific install instructions are located in:

* doc/INSTALL-LINUX.md
* doc/INSTALL-MAC.md
* doc/INSTALL-WINDOWS.md

## Getting Started

The best way to get started with libopenshot, is to learn about our build system, obtain all the source code, 
install a development IDE and tools, and better understand our dependencies. So, please read through the 
following sections, and follow the instructions. And keep in mind, that your computer is likely different 
than the one used when writing these instructions. Your file paths and versions of applications might be 
slightly different, so keep an eye out for subtle file path differences in the commands you type.

## Build Tools

CMake is the backbone of our build system.  It is a cross-platform build system, which checks for dependencies, 
locates header files and libraries, generates makefiles, and supports the cross-platform compiling of 
libopenshot and libopenshot-audio.  CMake uses an out-of-source build concept, where all temporary build 
files, such as makefiles, object files, and even the final binaries, are created outside of the source 
code folder, inside a /build/ sub-folder.  This prevents the build process from cluttering up the source 
code.  These instructions have only been tested with the GNU compiler (including MSYS2/MinGW for Windows).

## Dependencies

The following libraries are required to build libopenshot.  Instructions on how to install these 
dependencies vary for each operating system.  Libraries and Executables have been labeled in the 
list below to help distinguish between them.

* ### FFmpeg (libavformat, libavcodec, libavutil, libavdevice, libavresample, libswscale)
  * http://www.ffmpeg.org/ `(Library)`
  * This library is used to decode and encode video, audio, and image files.  It is also used to obtain information about media files, such as frame rate, sample rate, aspect ratio, and other common attributes.

* ### ImageMagick++ (libMagick++, libMagickWand, libMagickCore)
  * http://www.imagemagick.org/script/magick++.php `(Library)`
  * This library is **optional**, and used to decode and encode images.

* ### OpenShot Audio Library (libopenshot-audio)
  * https://github.com/OpenShot/libopenshot-audio/ `(Library)`
  * This library is used to mix, resample, host plug-ins, and play audio. It is based on the JUCE project, which is an outstanding audio library used by many different applications

* ### Qt 5 (libqt5)
  * http://www.qt.io/qt5/ `(Library)`
  * Qt5 is used to display video, store image data, composite images, apply image effects, and many other utility functions, such as file system manipulation, high resolution timers, etc...

* ### CMake (cmake)
  * http://www.cmake.org/ `(Executable)`
  * This executable is used to automate the generation of Makefiles, check for dependencies, and is the backbone of libopenshot’s cross-platform build process.

* ### SWIG (swig)
  * http://www.swig.org/ `(Executable)`
  * This executable is used to generate the Python and Ruby bindings for libopenshot. It is a simple and powerful wrapper for C++ libraries, and supports many languages.

* ### Python 3 (libpython)
  * http://www.python.org/ `(Executable and Library)`
  * This library is used by swig to create the Python (version 3+) bindings for libopenshot. This is also the official language used by OpenShot Video Editor (a graphical interface to libopenshot).

* ### Doxygen (doxygen)
  * http://www.stack.nl/~dimitri/doxygen/ `(Executable)`
  * This executable is used to auto-generate the documentation used by libopenshot.

* ### UnitTest++ (libunittest++)
  * https://github.com/unittest-cpp/ `(Library)`
  * This library is used to execute unit tests for libopenshot.  It contains many macros used to keep our unit testing code very clean and simple.

* ### ZeroMQ (libzmq)
  * http://zeromq.org/ `(Library)`
  * This library is used to communicate between libopenshot and other applications (publisher / subscriber). Primarily used to send debug data from libopenshot.

* ### OpenMP (-fopenmp)
  * http://openmp.org/wp/ `(Compiler Flag)`
  * If your compiler supports this flag (GCC, Clang, and most other compilers), it provides libopenshot with easy methods of using parallel programming techniques to improve performance and take advantage of multi-core processors.

## CMake Flags (Optional)
There are many different build flags that can be passed to cmake to adjust how libopenshot is compiled. Some of these flags might be required when compiling on certain OSes, just depending on how your build environment is setup. To add a build flag, follow this general syntax:  $ cmake -DMAGICKCORE_HDRI_ENABLE=1 -DENABLE_TESTS=1 ../

* MAGICKCORE_HDRI_ENABLE (default 0)
* MAGICKCORE_QUANTUM_DEPTH (default 0)
* OPENSHOT_IMAGEMAGICK_COMPATIBILITY (default 0)
* DISABLE_TESTS (default 0)
* CMAKE_PREFIX_PATH (`/location/to/missing/library/`)
* PYTHON_INCLUDE_DIR (`/location/to/python/include/`)
* PYTHON_LIBRARY (`/location/to/python/lib.a`)
* PYTHON_FRAMEWORKS (`/usr/local/Cellar/python3/3.3.2/Frameworks/Python.framework/`)
* CMAKE_CXX_COMPILER (`/location/to/mingw/g++`)
* CMAKE_C_COMPILER (`/location/to/mingw/gcc`)

## Obtaining Source Code

The first step in installing libopenshot is to obtain the most recent source code. The source code is available on [GitHub](https://github.com/OpenShot/libopenshot). Use the following command to obtain the latest libopenshot source code.

```
git clone https://github.com/OpenShot/libopenshot.git
git clone https://github.com/OpenShot/libopenshot-audio.git
```

## Folder Structure (libopenshot)

The source code is divided up into the following folders.

* ### build/
   * This folder needs to be manually created, and is used by cmake to store the temporary build files, such as makefiles, as well as the final binaries (library and test executables).

* ### cmake/
   * This folder contains custom modules not included by default in cmake, used to find dependency libraries and headers and determine if these libraries are installed.

* ### doc/
   * This folder contains documentation and related files, such as logos and images required by the doxygen auto-generated documentation.

* ### include/
   * This folder contains all headers (*.h) used by libopenshot.

* ### src/
   * This folder contains all source code (*.cpp) used by libopenshot.

* ### tests/
   * This folder contains all unit test code.  Each class has it’s own test file (*.cpp), and uses UnitTest++ macros to keep the test code simple and manageable.

* ### thirdparty/
   * This folder contains code not written by the OpenShot team. For example, jsoncpp, an open-source JSON parser.

## Linux Build Instructions (libopenshot-audio)
To compile libopenshot-audio, we need to go through a few additional steps to manually build and install it. Launch a terminal and enter:

```
cd [libopenshot-audio repo folder]
mkdir build
cd build
cmake ../
make
make install
./src/openshot-audio-test-sound  (This should play a test sound)
```

## Linux Build Instructions (libopenshot)
Run the following commands to compile libopenshot:

```
cd [libopenshot repo directory]
mkdir -p build
cd build
cmake ../
make
make install
```

For more detailed instructions, please see:

* doc/INSTALL-LINUX.md
* doc/INSTALL-MAC.md
* doc/INSTALL-WINDOWS.md
