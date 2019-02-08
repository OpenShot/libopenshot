## Getting Started

The best way to get started with libopenshot, is to learn about our build system, obtain all the source code, 
install a development IDE and tools, and better understand our dependencies. So, please read through the 
following sections, and follow the instructions. And keep in mind, that your computer is likely different 
than the one used when writing these instructions. Your file paths and versions of applications might be 
slightly different, so keep an eye out for subtle file path differences in the commands you type.

## Build Tools

CMake is the backbone of our build system.  It is a cross-platform build system, which checks for 
dependencies, locates header files and libraries, generates makefiles, and supports the cross-platform 
compiling of libopenshot and libopenshot-audio.  CMake uses an out-of-source build concept, where 
all temporary build files, such as makefiles, object files, and even the final binaries, are created 
outside of the source code folder, inside a /build/ sub-folder.  This prevents the build process 
from cluttering up the source code.  These instructions have only been tested with the GNU compiler 
(including MSYS2/MinGW for Windows).

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
There are many different build flags that can be passed to cmake to adjust how libopenshot is compiled. 
Some of these flags might be required when compiling on certain OSes, just depending on how your build 
environment is setup. To add a build flag, follow this general syntax: 
`cmake -DMAGICKCORE_HDRI_ENABLE=1 -DENABLE_TESTS=1 ../`

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

The first step in installing libopenshot is to obtain the most recent source code. The source code 
is available on [GitHub](https://github.com/OpenShot/libopenshot). Use the following command to 
obtain the latest libopenshot source code.

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

## Install Dependencies

In order to actually compile libopenshot and libopenshot-audio, we need to install some dependencies on 
your system. Most packages needed by libopenshot can be installed easily with Homebrew. However, first 
install Xcode with the following options ("UNIX Development", "System Tools", "Command Line Tools", or 
"Command Line Support"). Be sure to refresh your list of Homebrew packages with the “brew update” command.

**NOTE:** Homebrew seems to work much better for most users (compared to MacPorts), so I am going to 
focus on brew for this guide.

Install the following packages using the Homebrew package installer (http://brew.sh/). Pay close attention 
to any warnings or errors during these brew installs. NOTE: You might have some conflicting libraries in 
your /usr/local/ folders, so follow the directions from brew if these are detected.

```
brew install gcc48 --enable-all-languages
brew install ffmpeg
brew install librsvg
brew install swig
brew install doxygen
brew install unittest-cpp --cc=gcc-4.8. You must specify the c++ compiler with the --cc flag to be 4.7 or 4.8.
brew install qt5
brew install cmake
brew install zeromq
```

## Mac Build Instructions (libopenshot-audio)
Since libopenshot-audio is not available in a Homebrew or MacPorts package, we need to go through a 
few additional steps to manually build and install it. Launch a terminal and enter:

```
cd [libopenshot-audio repo folder]
mkdir build
cd build
cmake -d -G "Unix Makefiles" -DCMAKE_CXX_COMPILER=clang++ -DCMAKE_C_COMPILER=clang ../   (CLang must be used due to GNU incompatible Objective-C code in some of the Apple frameworks)
make
make install
./src/openshot-audio-test-sound  (This should play a test sound)
```

## Mac Build Instructions (libopenshot)
Run the following commands to build libopenshot:

```
$ cd [libopenshot repo folder]
$ mkdir build
$ cd build
$ cmake -G "Unix Makefiles"  -DCMAKE_CXX_COMPILER=/usr/local/opt/gcc48/bin/g++-4.8 -DCMAKE_C_COMPILER=/usr/local/opt/gcc48/bin/gcc-4.8 -DCMAKE_PREFIX_PATH=/usr/local/Cellar/qt5/5.4.2/ -DPYTHON_INCLUDE_DIR=/usr/local/Cellar/python3/3.3.2/Frameworks/Python.framework/Versions/3.3/include/python3.3m/ -DPYTHON_LIBRARY=/usr/local/Cellar/python3/3.3.2/Frameworks/Python.framework/Versions/3.3/lib/libpython3.3.dylib -DPython_FRAMEWORKS=/usr/local/Cellar/python3/3.3.2/Frameworks/Python.framework/ ../ -D"CMAKE_BUILD_TYPE:STRING=Debug"
```

The extra arguments on the cmake command make sure the compiler will be gcc4.8 and that cmake 
knows where to look for the Qt header files and Python library. Double check these file paths, 
as yours will likely be different.

```
make
```

If you are missing any dependencies for libopenshot, you will receive error messages at this point. 
Just install the missing dependencies, and run the above commands again. Repeat until no error 
messages are displayed and the build process completes.

Also, if you are having trouble building, please see the CMake Flags section above, as it might 
provide a solution for finding a missing folder path, missing Python 3 library, etc...

To run all unit tests (and verify everything is working correctly), launch a terminal, and enter:

```
make test
```

To auto-generate the documentation for libopenshot, launch a terminal, and enter:

```
make doc
```

This will use doxygen to generate a folder of HTML files, with all classes and methods documented. 
The folder is located at build/doc/html/. Once libopenshot has been successfully built, we need 
to install it (i.e. copy it to the correct folder, so other libraries can find it).

```
make install
```

This should copy the binary files to /usr/local/lib/, and the header files to /usr/local/include/openshot/... 
This is where other projects will look for the libopenshot files when building. Python 3 bindings are 
also installed at this point. let's verify the python bindings work:

```
python3 (or python)
>>> import openshot
```

If no errors are displayed, you have successfully compiled and installed libopenshot on your 
system. Congratulations and be sure to read our wiki on [Becoming an OpenShot Developer](https://github.com/OpenShot/openshot-qt/wiki/Become-a-Developer)! 
Welcome to the OpenShot developer community! We look forward to meeting you!
